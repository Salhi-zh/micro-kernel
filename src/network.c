#include <dns.h>
#include <endian.h>
#include <netif.h>
#include <network.h>
#include <rtl8139.h>
#include <serial.h>
#include <string.h>

#ifndef IP_PROTO_UDP
#define IP_PROTO_UDP 17
#endif

/* ARP cache */
#define ARP_CACHE_SIZE 16
typedef struct arp_entry {
  uint32_t ip;
  uint8_t mac[6];
  uint8_t state; /* 0 = free, 1 = incomplete, 2 = complete */
} arp_entry_t;
static arp_entry_t arp_cache[ARP_CACHE_SIZE];

static void arp_update(uint32_t ip, const uint8_t *mac) {
  for (int i = 0; i < ARP_CACHE_SIZE; i++) {
    if (arp_cache[i].state == 2 && arp_cache[i].ip == ip) {
      memcpy(arp_cache[i].mac, mac, 6);
      return;
    }
  }
  for (int i = 0; i < ARP_CACHE_SIZE; i++) {
    if (arp_cache[i].state == 0) {
      arp_cache[i].state = 2;
      arp_cache[i].ip = ip;
      memcpy(arp_cache[i].mac, mac, 6);
      return;
    }
  }
  // Fallback if full: overwrite slot 0
  arp_cache[0].ip = ip;
  memcpy(arp_cache[0].mac, mac, 6);
}

static bool arp_lookup(uint32_t ip, uint8_t *mac_out) {
  for (int i = 0; i < ARP_CACHE_SIZE; i++) {
    if (arp_cache[i].state == 2 && arp_cache[i].ip == ip) {
      memcpy(mac_out, arp_cache[i].mac, 6);
      return true;
    }
  }
  return false;
}

/* --- TCP Control Blocks (Stubs) --- */
#define TCP_CB_SIZE 8
typedef struct tcb {
  uint32_t local_ip;
  uint16_t local_port;
  uint32_t remote_ip;
  uint16_t remote_port;
  uint32_t snd_nxt;
  uint32_t rcv_nxt;
  uint8_t state;
} tcb_t;
static tcb_t tcbs[TCP_CB_SIZE];
void tcp_listen(uint16_t port) {
  (void)port;
  (void)tcbs;
}

/* --- Globals --- */
static icmp_reply_callback_t icmp_reply_cb = NULL;
volatile uint32_t resolved_dns_ip = 0;
const uint32_t GATEWAY_IP = 0x0A000201; // 10.0.2.1

void network_register_icmp_reply_callback(icmp_reply_callback_t cb) {
  icmp_reply_cb = cb;
}

static void net_send(const uint8_t *frame, uint16_t len) {
  netif_t *netif = netif_get_default();
  if (netif && netif->send)
    netif->send(netif, frame, len);
}

/* --- ARP Logic --- */
void arp_request(uint32_t target_ip) {
  netif_t *netif = netif_get_default();
  if (!netif)
    return;
  uint8_t req[42];

  memset(req, 0xFF, 6);              // Dest MAC: Broadcast
  memcpy(req + 6, netif->hwaddr, 6); // Src MAC: Ours
  req[12] = 0x08;
  req[13] = 0x06; // EtherType: ARP

  req[14] = 0x00;
  req[15] = 0x01; // HTYPE (Eth)
  req[16] = 0x08;
  req[17] = 0x00; // PTYPE (IPv4)
  req[18] = 0x06;
  req[19] = 0x04; // HLEN, PLEN
  req[20] = 0x00;
  req[21] = 0x01; // OPER: Request (1)

  memcpy(req + 22, netif->hwaddr, 6); // SHA
  uint32_t my_ip = htobe32(netif->ipaddr);
  memcpy(req + 28, &my_ip, 4); // SPA
  memset(req + 32, 0x00, 6);   // THA (Empty)
  uint32_t tip = htobe32(target_ip);
  memcpy(req + 38, &tip, 4); // TPA

  net_send(req, 42);
  serial_puts(COM1, "network: Broadcasted ARP Request\n");
}

static void arp_handle(const uint8_t *frame, uint16_t len) {
  if (len < 42)
    return;
  netif_t *netif = netif_get_default();
  if (!netif)
    return;

  uint16_t oper = (frame[20] << 8) | frame[21];
  uint32_t sender_ip =
      (frame[28] << 24) | (frame[29] << 16) | (frame[30] << 8) | frame[31];
  uint32_t target_ip =
      (frame[38] << 24) | (frame[39] << 16) | (frame[40] << 8) | frame[41];

  arp_update(sender_ip, frame + 22); // Update cache dynamically

  if (oper == 1 && target_ip == netif->ipaddr) {
    uint8_t reply[42];
    memcpy(reply, frame + 6, 6);
    memcpy(reply + 6, netif->hwaddr, 6);
    reply[12] = 0x08;
    reply[13] = 0x06;
    reply[14] = 0x00;
    reply[15] = 0x01;
    reply[16] = 0x08;
    reply[17] = 0x00;
    reply[18] = 0x06;
    reply[19] = 0x04;
    reply[20] = 0x00;
    reply[21] = 0x02; // Reply (2)
    memcpy(reply + 22, netif->hwaddr, 6);
    memcpy(reply + 28, frame + 38, 4);
    memcpy(reply + 32, frame + 22, 6);
    memcpy(reply + 38, frame + 28, 4);
    net_send(reply, 42);
  }
}

/* --- IPv4 Outbound --- */
void ipv4_send(uint32_t dest_ip, uint8_t protocol, const uint8_t *data,
               uint16_t len) {
  netif_t *netif = netif_get_default();
  if (!netif)
    return;

  uint32_t next_hop = dest_ip;
  uint32_t netmask = 0xFFFFFF00; // 255.255.255.0 subnet

  // Route outside subnet to gateway
  if ((dest_ip & netmask) != (netif->ipaddr & netmask)) {
    next_hop = GATEWAY_IP;
  }

  uint8_t dest_mac[6];
  if (!arp_lookup(next_hop, dest_mac)) {
    arp_request(next_hop);
    serial_puts(COM1, "ipv4_send: Missing ARP for hop, dropping packet\n");
    return;
  }

  uint16_t total_len = 14 + 20 + len;
  uint8_t frame[1514];
  memcpy(frame, dest_mac, 6);
  memcpy(frame + 6, netif->hwaddr, 6);
  frame[12] = 0x08;
  frame[13] = 0x00; // IPv4

  uint8_t *ip = frame + 14;
  ip[0] = 0x45;
  ip[1] = 0;
  uint16_t ip_len = 20 + len;
  ip[2] = ip_len >> 8;
  ip[3] = ip_len & 0xFF;
  ip[4] = 0;
  ip[5] = 0;
  ip[6] = 0x40;
  ip[7] = 0;  // DF
  ip[8] = 64; // TTL
  ip[9] = protocol;
  ip[10] = 0;
  ip[11] = 0;

  uint32_t my_ip = htobe32(netif->ipaddr);
  memcpy(ip + 12, &my_ip, 4);
  uint32_t dip = htobe32(dest_ip);
  memcpy(ip + 16, &dip, 4);

  uint32_t csum = 0;
  for (int i = 0; i < 10; i++)
    csum += (ip[i * 2] << 8) | ip[i * 2 + 1];
  while (csum >> 16)
    csum = (csum & 0xFFFF) + (csum >> 16);
  csum = ~csum;
  ip[10] = csum >> 8;
  ip[11] = csum & 0xFFFF;

  memcpy(frame + 34, data, len);
  net_send(frame, total_len);
}

/* --- ICMP Logic --- */
void icmp_request(uint32_t dest_ip, uint16_t seq) {
  uint8_t icmp[64];
  memset(icmp, 0, 64);
  icmp[0] = 8; // Echo Request
  icmp[4] = 0x42;
  icmp[5] = 0x42; // Identifier
  icmp[6] = seq >> 8;
  icmp[7] = seq & 0xFF; // Seq

  for (int i = 8; i < 64; i++)
    icmp[i] = i; // Dummy Payload

  uint32_t csum = 0;
  for (int i = 0; i < 64; i += 2)
    csum += (icmp[i] << 8) | icmp[i + 1];
  while (csum >> 16)
    csum = (csum & 0xFFFF) + (csum >> 16);
  csum = ~csum;
  icmp[2] = csum >> 8;
  icmp[3] = csum & 0xFFFF;

  ipv4_send(dest_ip, IP_PROTO_ICMP, icmp, 64);
}

static void icmp_handle(uint32_t src_ip, const uint8_t *payload,
                        uint16_t payload_len) {
  if (payload_len < 8)
    return;
  uint8_t type = payload[0];
  uint8_t code = payload[1];

  if (type == 8 && code == 0) { // Ping Request
    uint8_t reply[1500];
    memcpy(reply, payload, payload_len);
    reply[0] = 0; // Echo Reply
    reply[2] = 0;
    reply[3] = 0; // Clear CSUM
    uint32_t csum = 0;
    for (int i = 0; i < payload_len - 1; i += 2)
      csum += (reply[i] << 8) | reply[i + 1];
    if (payload_len & 1)
      csum += (reply[payload_len - 1] << 8);
    while (csum >> 16)
      csum = (csum & 0xFFFF) + (csum >> 16);
    csum = ~csum;
    reply[2] = csum >> 8;
    reply[3] = csum & 0xFFFF;

    ipv4_send(src_ip, IP_PROTO_ICMP, reply, payload_len);
  } else if (type == 0 && code == 0) { // Ping Reply
    uint16_t seq = (payload[6] << 8) | payload[7];
    if (icmp_reply_cb)
      icmp_reply_cb(src_ip, seq);
  }
}

/* --- UDP & DNS Stack --- */

void udp_send(uint32_t dest_ip, uint16_t src_port, uint16_t dst_port,
              const uint8_t *data, uint16_t len) {
  netif_t *netif = netif_get_default();
  if (!netif)
    return;

  uint16_t udp_len = 8 + len;
  uint8_t udp[1500];

  udp[0] = src_port >> 8;
  udp[1] = src_port & 0xFF;
  udp[2] = dst_port >> 8;
  udp[3] = dst_port & 0xFF;
  udp[4] = udp_len >> 8;
  udp[5] = udp_len & 0xFF;
  udp[6] = 0;
  udp[7] = 0; // Initialize checksum slot

  memcpy(udp + 8, data, len);

  // Bullet-proof UDP Checksum including IPv4 pseudo-header
  uint32_t csum = 0;
  uint8_t pseudo[12];
  uint32_t src = htobe32(netif->ipaddr);
  uint32_t dst = htobe32(dest_ip);

  memcpy(pseudo, &src, 4);
  memcpy(pseudo + 4, &dst, 4);
  pseudo[8] = 0;
  pseudo[9] = IP_PROTO_UDP;
  pseudo[10] = udp_len >> 8;
  pseudo[11] = udp_len & 0xFF;

  for (int i = 0; i < 12; i += 2)
    csum += (pseudo[i] << 8) | pseudo[i + 1];
  for (int i = 0; i < udp_len - 1; i += 2)
    csum += (udp[i] << 8) | udp[i + 1];
  if (udp_len & 1)
    csum += (udp[udp_len - 1] << 8);

  while (csum >> 16)
    csum = (csum & 0xFFFF) + (csum >> 16);
  csum = ~csum;
  if (csum == 0)
    csum = 0xFFFF;

  udp[6] = csum >> 8;
  udp[7] = csum & 0xFFFF;

  ipv4_send(dest_ip, IP_PROTO_UDP, udp, udp_len);
}

// Built-in rock-solid DNS Requester
void network_dns_request(const char *domain) {
  uint8_t payload[256];
  memset(payload, 0, sizeof(payload));

  payload[0] = 0x13;
  payload[1] = 0x37; // Transaction ID
  payload[2] = 0x01;
  payload[3] = 0x00; // Flags: Standard Query
  payload[4] = 0x00;
  payload[5] = 0x01; // Questions: 1

  uint16_t offset = 12;
  const char *p = domain;

  // Encode QNAME ("google.com" -> "\x06google\x03com\x00")
  while (*p) {
    const char *dot = strchr(p, '.');
    if (!dot)
      dot = p + strlen(p);
    uint8_t len = dot - p;
    payload[offset++] = len;
    memcpy(&payload[offset], p, len);
    offset += len;
    if (*dot == '.')
      p = dot + 1;
    else
      break;
  }
  payload[offset++] = 0x00; // Null terminator

  payload[offset++] = 0x00;
  payload[offset++] = 0x01; // QTYPE: A (IPv4)
  payload[offset++] = 0x00;
  payload[offset++] = 0x01; // QCLASS: IN

  uint32_t dns_server = 0x08080808; // 8.8.8.8
  udp_send(dns_server, 12345, 53, payload, offset);
  serial_puts(COM1, "network: Sent DNS Request\n");
}

extern void dns_handle(const uint8_t *payload, uint16_t len);

static void udp_handle(uint32_t src_ip, const uint8_t *payload, uint16_t len) {
  (void)src_ip;
  if (len < 8)
    return;
  uint16_t dst_port = (payload[2] << 8) | payload[3];
  uint16_t udp_len = (payload[4] << 8) | payload[5];

  if (dst_port == 12345) {
    const uint8_t *dns = payload + 8;
    uint16_t dns_len = udp_len - 8;

    // Inline Aggressive DNS A-Record Parser
    if (dns_len >= 12) {
      uint16_t flags = (dns[2] << 8) | dns[3];
      if (flags & 0x8000) { // Is Response
        uint16_t qdcount = (dns[4] << 8) | dns[5];
        uint16_t ancount = (dns[6] << 8) | dns[7];
        if (ancount > 0) {
          uint16_t offset = 12;
          // Skip Questions segment
          for (int i = 0; i < qdcount && offset < dns_len; i++) {
            while (dns[offset] != 0 && offset < dns_len) {
              if ((dns[offset] & 0xC0) == 0xC0) {
                offset += 2;
                goto q_done;
              } else
                offset += dns[offset] + 1;
            }
            offset++;
          q_done:
            offset += 4;
          }
          // Read first Answer
          if (offset + 12 <= dns_len) {
            if ((dns[offset] & 0xC0) == 0xC0)
              offset += 2;
            uint16_t atype = (dns[offset] << 8) | dns[offset + 1];
            uint16_t rdlen = (dns[offset + 8] << 8) | dns[offset + 9];
            offset += 10;
            if (atype == 1 && rdlen == 4) {
              uint32_t ip;
              memcpy(&ip, dns + offset, 4);
              resolved_dns_ip = be32toh(ip); // Save globally natively
              serial_puts(COM1, "network: DNS Successfully Resolved IP!\n");
            }
          }
        }
      }
    }

    // Pass to external handler too (if it exists)
    dns_handle(payload + 8, udp_len - 8);
  }
}

/* --- Packet Demux --- */
void network_receive(const uint8_t *frame, uint16_t len) {
  if (len < 14 || len > 1514)
    return;
  uint16_t eth_type = (frame[12] << 8) | frame[13];

  if (eth_type == ETH_TYPE_ARP) {
    arp_handle(frame, len);
  } else if (eth_type == ETH_TYPE_IP) {
    const uint8_t *ip = frame + 14;
    uint8_t ihl = ip[0] & 0x0F;
    uint8_t proto = ip[9];
    uint32_t src_ip;
    memcpy(&src_ip, ip + 12, 4);
    src_ip = be32toh(src_ip);

    const uint8_t *payload = ip + (ihl * 4);
    uint16_t payload_len = ((ip[2] << 8) | ip[3]) - (ihl * 4);

    if (proto == IP_PROTO_ICMP) {
      icmp_handle(src_ip, payload, payload_len);
    } else if (proto == IP_PROTO_UDP) {
      udp_handle(src_ip, payload, payload_len);
    }
  }
}
