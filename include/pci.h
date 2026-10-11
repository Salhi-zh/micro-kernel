
#ifndef PCI_H
#define PCI_H

#include <kernel.h>

typedef struct pci_dev {
    uint16_t bus;
    uint16_t slot;
    uint16_t func;
    uint16_t vendor_id;
    uint16_t device_id;
    uint8_t  class_code;
    uint8_t  subclass;
    uint8_t  prog_if;
    uint32_t bar0;
    uint32_t bar1;
    uint32_t bar2;
    uint32_t bar3;
    uint32_t bar4;
    uint32_t bar5;
    uint8_t  irq;
} pci_dev_t;

void pci_init(void);
pci_dev_t* pci_find_device(uint16_t vendor, uint16_t device);
uint32_t pci_read_config(uint16_t bus, uint16_t slot, uint16_t func, uint16_t offset);
void pci_write_config(uint16_t bus, uint16_t slot, uint16_t func, uint16_t offset, uint32_t value);

#endif
