
#ifndef DNS_H
#define DNS_H

#include <kernel.h>

// Resolves a hostname (e.g., "google.com") to an IPv4 address.
// Returns the IP in Host Byte Order, or 0 on failure.
uint32_t dns_resolve(const char* hostname);

// Parses an incoming UDP DNS packet (payload starts at DNS header)
void dns_handle_packet(const uint8_t* payload, uint16_t len);

#endif
