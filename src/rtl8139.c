
#include <rtl8139.h>
#include <pci.h>
#include <irq.h>
#include <network.h>
#include <string.h>
#include <serial.h>
#include <kernel.h>

#define RTL8139_IDR0      0x00
#define RTL8139_TSD0      0x10
#define RTL8139_TSAD0     0x20
#define RTL8139_RBSTART   0x30
#define RTL8139_CR        0x37
#define RTL8139_CAPR      0x38
#define RTL8139_IMR       0x3C
#define RTL8139_ISR       0x3E
#define RTL8139_RCR       0x44
#define RTL8139_CONFIG1   0x52

#define RTL_CMD_RESET      0x10
#define RTL_CMD_RX_ENABLE  0x08
#define RTL_CMD_TX_ENABLE  0x04
#define RTL_CMD_RX_BUF_EMPTY 0x01

#define RTL_ISR_ROK        0x01
#define RTL_ISR_TOK        0x04

static netif_t rtl8139_iface;
static uint32_t io_base;

/* * Le buffer de réception fait 8K. On ajoute 16 octets d'entête 
 * + 1500 octets (WRAP) pour permettre à la carte de déborder 
 * proprement si un paquet est à cheval sur la fin des 8K.
 */
static uint8_t rx_buffer[8192 + 16 + 1500] __attribute__((aligned(4)));
static uint16_t rx_cur = 0;

/* 4 buffers statiques pour l'envoi (DMA sécurisé) */
static uint8_t tx_buffers[4][2048] __attribute__((aligned(4)));
static uint8_t tx_cur = 0;

static void rtl8139_transmit(struct netif* iface, const uint8_t* packet, uint16_t len) 
{
    (void)iface;
    if (len > 1514) return;

    /* 1. On copie les données de la pile vers un buffer statique sécurisé */
    memcpy(tx_buffers[tx_cur], packet, len);

    /* 2. On pad à 60 octets (Taille minimale stricte pour Ethernet/ARP) */
    if (len < 60) {
        memset(tx_buffers[tx_cur] + len, 0, 60 - len);
        len = 60;
    }

    serial_puts(COM1, "RTL8139 TX\n");

    /* 3. On fournit l'adresse physique et on déclenche l'envoi */
    outl(io_base + RTL8139_TSAD0 + (tx_cur * 4), (uint32_t)tx_buffers[tx_cur]);
    outl(io_base + RTL8139_TSD0 + (tx_cur * 4), len);

    tx_cur = (tx_cur + 1) % 4;
}

void rtl8139_poll_receive(void) 
{
    /* Tant que le bit "Buffer Empty" (0x01) de CR n'est pas à 1 */
    while ((inb(io_base + RTL8139_CR) & RTL_CMD_RX_BUF_EMPTY) == 0) {
        
        uint16_t* rx_ptr = (uint16_t*)(rx_buffer + rx_cur);
        uint16_t status = rx_ptr[0];
        uint16_t len = rx_ptr[1]; // Longueur reçue (inclut l'entête + 4 octets de CRC !!)

        // Acquittement des bits dans l'ISR. Requis pour débloquer la machine à états de QEMU.
        outw(io_base + RTL8139_ISR, RTL_ISR_ROK | RTL_ISR_TOK);

        // Sanity check au cas où on lit des résidus
        if (len == 0 || len > 8192) {
            serial_puts(COM1, "RTL8139 RX ERR: Invalid len\n");
            break; 
        }

        if (status & RTL_ISR_ROK) {
            serial_puts(COM1, "RTL8139 RX OK\n");
            /* On donne le paquet à la couche réseau (len - 4 pour enlever le CRC) */
            uint8_t* packet_data = (uint8_t*)rx_ptr + 4;
            network_receive(packet_data, len - 4);
        }

        /* Calcul magique RTL8139 : 
         * Avancer de len + 4 (entête), puis arrondir au multiple de 4 (dword) supérieur */
        rx_cur = (rx_cur + len + 4 + 3) & ~3;

        /* Gestion de la boucle (Wrap-around) */
        if (rx_cur >= 8192) {
            rx_cur -= 8192;
        }

        /* Mise à jour du pointeur matériel (-16 car spécification RTL) */
        outw(io_base + RTL8139_CAPR, rx_cur - 16);
    }
}

void rtl8139_init(pci_dev_t *pci_dev) 
{
    io_base = pci_dev->bar0 & ~3; 

    // OBLIGATOIRE : Activer le PCI Bus Mastering (Autorise le DMA pour le périphérique)
    uint32_t pci_cmd = pci_read_config(pci_dev->bus, pci_dev->slot, pci_dev->func, 0x04);
    pci_write_config(pci_dev->bus, pci_dev->slot, pci_dev->func, 0x04, pci_cmd | 0x04);

    // Allumer la carte
    outb(io_base + RTL8139_CONFIG1, 0x00);

    // Reset complet (attend que le bit retombe à 0)
    outb(io_base + RTL8139_CR, RTL_CMD_RESET);
    while (inb(io_base + RTL8139_CR) & RTL_CMD_RESET);

    // Configuration Receive Buffer
    rx_cur = 0;
    outl(io_base + RTL8139_RBSTART, (uint32_t)rx_buffer);

    // Initialiser le pointeur de lecture matériel pour être synchro à 0xFFF0
    outw(io_base + RTL8139_CAPR, 0xFFF0);

    // RCR: AB+AM+AP+AAP = 0x0F (Accepte tout).
    // WRAP = 0x80 (Permet aux paquets de déborder dans les +1500 octets pour faciliter la lecture)
    outl(io_base + RTL8139_RCR, 0x8F); 

    // Activer les masques d'interruptions (Essentiel sous QEMU même si on poll en software !)
    outw(io_base + RTL8139_IMR, RTL_ISR_ROK | RTL_ISR_TOK);

    // Activer l'envoi et la réception
    outb(io_base + RTL8139_CR, RTL_CMD_RX_ENABLE | RTL_CMD_TX_ENABLE);

    // Lire l'adresse MAC
    for (int i = 0; i < 6; i++) {
        rtl8139_iface.hwaddr[i] = inb(io_base + RTL8139_IDR0 + i);
    }

    // Assigner la fonction d'envoi correcte
    rtl8139_iface.send = rtl8139_transmit;

    serial_puts(COM1, "RTL8139 initialized successfully.\n");
}

netif_t* rtl8139_get_netif(void) 
{
    return &rtl8139_iface;
}
