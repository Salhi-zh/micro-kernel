
#include <netif.h>
#include <rtl8139.h>
#include <serial.h>

static uint32_t kernel_ip = 0x0A00020F; // 10.0.2.15

netif_t* netif_get_default(void)
{
    netif_t* iface = rtl8139_get_netif();
    if (iface) {
        iface->ipaddr = kernel_ip;
    }
    return iface;
}

void netif_set_ip(uint32_t ip)
{
    serial_puts(COM1, "netif_set_ip: setting to ");
    // crude hex print for debugging
    serial_putchar(COM1, '0' + ((ip >> 28) & 0xF));
    serial_putchar(COM1, '0' + ((ip >> 24) & 0xF));
    serial_putchar(COM1, '0' + ((ip >> 20) & 0xF));
    serial_putchar(COM1, '0' + ((ip >> 16) & 0xF));
    serial_putchar(COM1, '0' + ((ip >> 12) & 0xF));
    serial_putchar(COM1, '0' + ((ip >> 8) & 0xF));
    serial_putchar(COM1, '0' + ((ip >> 4) & 0xF));
    serial_putchar(COM1, '0' + (ip & 0xF));
    serial_puts(COM1, "\n");

    kernel_ip = ip;
    netif_t* iface = rtl8139_get_netif();
    if (iface) {
        iface->ipaddr = kernel_ip;
    }
}
