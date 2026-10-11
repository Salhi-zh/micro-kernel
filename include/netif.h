// include/netif.h
#ifndef NETIF_H
#define NETIF_H

#include <kernel.h>

typedef struct netif {
    uint8_t hwaddr[6];       /* MAC address */
    uint32_t ipaddr;         /* in network byte order */
    void (*send)(struct netif* iface, const uint8_t* packet, uint16_t len);
    /* Receive would typically be called from interrupt, we stub it */
} netif_t;

void netif_init(void);
netif_t* netif_get_default(void);
void netif_set_ip(uint32_t ip);
#endif
