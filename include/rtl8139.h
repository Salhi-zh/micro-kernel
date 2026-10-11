
#ifndef RTL8139_H
#define RTL8139_H

#include <kernel.h>
#include <netif.h>
#include <pci.h>

void rtl8139_init(pci_dev_t *pci_dev);
netif_t* rtl8139_get_netif(void);
void rtl8139_poll_receive(void);   // new

#endif
