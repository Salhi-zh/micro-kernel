
#include <dns.h>
#include <network.h>
#include <rtl8139.h>
#include <endian.h>
#include <string.h>

#define DNS_SERVER_IP 0x08080808 // 8.8.8.8
#define IP_PROTO_UDP 17

static volatile uint32_t dns_resolved_ip = 0;
static volatile bool dns_response_received = false;

struct dns_header {
    uint16_t id;
    uint16_t flags;
    uint16_t qdcount;
    uint16_t ancount;
    uint16_t nscount;
    uint16_t arcount;
} __attribute__((packed));

// Simple UDP Sender
static void udp_send(uint32_t dest_ip, uint16_t src_port, uint16_t dest_port, const uint8_t* data, uint16_t len)
{
    uint16_t udp_len = 8 + len;
    uint8_t buffer[256]; 
    uint16_t* udp = (uint16_t*)buffer;

    // UDP Header
    udp[0] = htobe16(src_port);
    udp[1] = htobe16(dest_port);
    udp[2] = htobe16(udp_len);
    udp[3] = 0; // Checksum is optional in IPv4, leaving it 0

    // Copy payload and send down to IPv4 layer
    memcpy(buffer + 8, data, len);
    ipv4_send(dest_ip, IP_PROTO_UDP, buffer, udp_len);
}

// Safely skip a DNS name (handles pointers and labels without out-of-bounds crashes)
static void skip_name(const uint8_t** p, const uint8_t* end)
{
    while (*p < end && **p != 0) {
        if ((**p & 0xC0) == 0xC0) {
            *p += 2; // Pointer is 2 bytes, covers the rest of the name
            return;
        }
        *p += (**p) + 1; // Skip label length + label bytes
    }
    if (*p < end) *p += 1; // Skip the terminating null byte
}

void dns_handle(const uint8_t* payload, uint16_t len)
{
    if (len < sizeof(struct dns_header)) return;
    const uint8_t* end = payload + len;

    struct dns_header* dns = (struct dns_header*)payload;
    uint16_t flags = be16toh(dns->flags);

    // Check if it's a response (QR bit = 1)
    if ((flags & 0x8000) == 0) return;

    uint16_t qdcount = be16toh(dns->qdcount);
    uint16_t ancount = be16toh(dns->ancount);

    // Check for error codes in lower 4 bits, or 0 answers
    if ((flags & 0x000F) != 0 || ancount == 0) {
        dns_response_received = true;
        return;
    }

    const uint8_t* p = payload + sizeof(struct dns_header);

    // Skip the questions section
    for (int i = 0; i < qdcount; i++) {
        skip_name(&p, end);
        p += 4; // Skip QTYPE and QCLASS
        if (p > end) return;
    }

    // Parse answers section
    for (int i = 0; i < ancount; i++) {
        skip_name(&p, end);
        if (p + 10 > end) return;

        uint16_t atype = (p[0] << 8) | p[1];
        // class is p[2..3], ttl is p[4..7]
        uint16_t rdlength = (p[8] << 8) | p[9];
        p += 10; // Skip type, class, ttl, and rdlength

        if (p + rdlength > end) return;

        // If it's an IPv4 A record (Type 1) and length is 4 bytes
        if (atype == 1 && rdlength == 4) {
            uint32_t ip;
            memcpy(&ip, p, 4);
            dns_resolved_ip = be32toh(ip); // Convert to Host Byte Order
            dns_response_received = true;
            return;
        }

        p += rdlength; // Skip over RDATA to look at next answer
    }

    dns_response_received = true; // Received, but no A record found
}

uint32_t dns_resolve(const char* hostname)
{
    uint8_t buf[256];
    struct dns_header* dns = (struct dns_header*)buf;
    memset(dns, 0, sizeof(struct dns_header));

    dns->id = htobe16(0xABCD);
    dns->flags = htobe16(0x0100); // Standard query, recursion desired
    dns->qdcount = htobe16(1);

    uint8_t* qname = buf + sizeof(struct dns_header);
    const char* host_ptr = hostname;
    uint8_t* label_len_ptr = qname++;
    uint8_t label_len = 0;

    // Convert string "google.com" to DNS format "\x06google\x03com\x00"
    while (*host_ptr) {
        if (*host_ptr == '.') {
            *label_len_ptr = label_len;
            label_len = 0;
            label_len_ptr = qname++;
        } else {
            *qname++ = *host_ptr;
            label_len++;
        }
        host_ptr++;
    }
    *label_len_ptr = label_len;
    *qname++ = 0; // Terminate QNAME

    // QTYPE = 1 (A record)
    *qname++ = 0;
    *qname++ = 1;

    // QCLASS = 1 (IN)
    *qname++ = 0;
    *qname++ = 1;

    uint16_t query_len = qname - buf;

    // Reset state
    dns_resolved_ip = 0;
    dns_response_received = false;

    // Send DNS request via UDP to 8.8.8.8 port 53 (using ephemeral port 12345)
    udp_send(DNS_SERVER_IP, 12345, 53, buf, query_len);

    // Poll NIC for response (timeout ~500ms depending on VM speed)
    volatile int timeout = 1000000;
    while (!dns_response_received && timeout > 0) {
        rtl8139_poll_receive();
        timeout--;
    }

    return dns_resolved_ip;
}
