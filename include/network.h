
#ifndef NETWORK_H
#define NETWORK_H

#include <kernel.h>

#define ETH_TYPE_IP  0x0800
#define ETH_TYPE_ARP 0x0806
#define IP_PROTO_ICMP 1
#define IP_PROTO_TCP  6
#define IP_PROTO_UDP  17

typedef void (*icmp_reply_callback_t)(uint32_t src_ip, uint16_t seq);

void network_receive(const uint8_t* frame, uint16_t len);
void arp_request(uint32_t ip);
void ipv4_send(uint32_t dest_ip, uint8_t protocol, const uint8_t* data, uint16_t len);
void tcp_listen(uint16_t port);
void network_register_icmp_reply_callback(icmp_reply_callback_t cb);

/* New ICMP and DNS functions */
void icmp_request(uint32_t dest_ip, uint16_t seq);
uint32_t dns_resolve(const char* domain);

#endif
