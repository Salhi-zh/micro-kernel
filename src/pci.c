
#include <pci.h>
#include <kernel.h>
#include <serial.h>

#define CONFIG_ADDRESS 0xCF8
#define CONFIG_DATA    0xCFC

static uint32_t pci_make_addr(uint16_t bus, uint16_t slot, uint16_t func, uint16_t offset)
{
    return (uint32_t)((bus << 16) | (slot << 11) | (func << 8) | (offset & 0xFC) | 0x80000000);
}

uint32_t pci_read_config(uint16_t bus, uint16_t slot, uint16_t func, uint16_t offset)
{
    outl(CONFIG_ADDRESS, pci_make_addr(bus, slot, func, offset));
    return inl(CONFIG_DATA);
}

void pci_write_config(uint16_t bus, uint16_t slot, uint16_t func, uint16_t offset, uint32_t value)
{
    outl(CONFIG_ADDRESS, pci_make_addr(bus, slot, func, offset));
    outl(CONFIG_DATA, value);
}

static pci_dev_t pci_devices[32];
static int pci_device_count = 0;

void pci_init(void)
{
    pci_device_count = 0;
    for (uint16_t bus = 0; bus < 256; bus++) {
        for (uint16_t slot = 0; slot < 32; slot++) {
            for (uint16_t func = 0; func < 8; func++) {
                uint32_t id = pci_read_config(bus, slot, func, 0);
                uint16_t vendor = id & 0xFFFF;
                if (vendor == 0xFFFF) continue; // no device

                pci_dev_t *dev = &pci_devices[pci_device_count++];
                dev->bus = bus;
                dev->slot = slot;
                dev->func = func;
                dev->vendor_id = vendor;
                dev->device_id = id >> 16;

                uint32_t class_rev = pci_read_config(bus, slot, func, 0x08);
                dev->class_code = (class_rev >> 24) & 0xFF;
                dev->subclass   = (class_rev >> 16) & 0xFF;
                dev->prog_if    = (class_rev >> 8) & 0xFF;

                // Read BARs
                dev->bar0 = pci_read_config(bus, slot, func, 0x10);
                dev->bar1 = pci_read_config(bus, slot, func, 0x14);
                dev->bar2 = pci_read_config(bus, slot, func, 0x18);
                dev->bar3 = pci_read_config(bus, slot, func, 0x1C);
                dev->bar4 = pci_read_config(bus, slot, func, 0x20);
                dev->bar5 = pci_read_config(bus, slot, func, 0x24);

                uint32_t irq_line = pci_read_config(bus, slot, func, 0x3C);
                dev->irq = irq_line & 0xFF;
            }
        }
    }
}

pci_dev_t* pci_find_device(uint16_t vendor, uint16_t device)
{
    for (int i = 0; i < pci_device_count; i++) {
        if (pci_devices[i].vendor_id == vendor && pci_devices[i].device_id == device)
            return &pci_devices[i];
    }
    return NULL;
}
