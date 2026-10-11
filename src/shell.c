
#include <kernel.h>
#include <vga.h>
#include <keyboard.h>
#include <network.h>
#include <netif.h>
#include <string.h>
#include <serial.h>
#include <rtl8139.h> // for poll

static bool ping_received[16];   // track which sequence numbers have been received

static void vga_print_dec(uint32_t val)
{
    char buf[12];
    int i = 0;
    if (val == 0) {
        vga_putchar('0');
        return;
    }
    while (val > 0) {
        buf[i++] = '0' + (val % 10);
        val /= 10;
    }
    while (i > 0) {
        vga_putchar(buf[--i]);
    }
}

static void vga_print_ip(uint32_t ip)
{
    uint8_t* p = (uint8_t*)&ip;
    vga_print_dec(p[3]);
    vga_putchar('.');
    vga_print_dec(p[2]);
    vga_putchar('.');
    vga_print_dec(p[1]);
    vga_putchar('.');
    vga_print_dec(p[0]);
}

static uint32_t parse_ip(const char* s)
{
    uint32_t ip = 0;
    int shift = 24;
    serial_puts(COM1, "parse_ip: entered with string: ");
    serial_puts(COM1, s);
    serial_puts(COM1, "\n");
    
    for (int i = 0; s[i] != '\0'; i++) {
        if ((s[i] >= 'a' && s[i] <= 'z') || (s[i] >= 'A' && s[i] <= 'Z')) {
            // Call our new blocking resolver
            return dns_resolve(s); 
        }
    }
    while (*s) {
        // Skip any spaces
        while (*s == ' ') s++;
        if (*s == '\0') break;

        uint32_t octet = 0;
        
        // Break if we hit unexpected characters to prevent infinite loops
        if (*s < '0' || *s > '9') break;

        while (*s >= '0' && *s <= '9') {
            octet = octet * 10 + (*s - '0');
            s++;
        }
        ip |= (octet << shift);
        shift -= 8;
        
        if (*s == '.') {
            s++;
        } else {
            // Reached the end of the IP string (e.g., hit space or \0)
            break;
        }
    }
    return ip;
}

// Standard Internet Checksum for ICMP
static uint16_t icmp_checksum(void *b, int len)
{
    uint16_t *buf = b;
    unsigned int sum = 0;
    uint16_t result;

    for (sum = 0; len > 1; len -= 2)
        sum += *buf++;
    if (len == 1)
        sum += *(unsigned char*)buf;
    
    sum = (sum >> 16) + (sum & 0xFFFF);
    sum += (sum >> 16);
    result = ~sum;
    return result;
}

static uint32_t ping_target = 0;
static volatile int ping_replies = 0;
static int ping_sent = 0;
static int ping_total = 4;

static void icmp_reply_callback(uint32_t src_ip, uint16_t seq)
{
    serial_puts(COM1, "icmp_reply_callback: src_ip=");
    uint8_t* p = (uint8_t*)&src_ip;
    serial_putchar(COM1, '0' + p[3] / 100);
    serial_putchar(COM1, '0' + (p[3] / 10) % 10);
    serial_putchar(COM1, '0' + p[3] % 10);
    serial_putchar(COM1, '.');
    serial_putchar(COM1, '0' + p[2] / 100);
    serial_putchar(COM1, '0' + (p[2] / 10) % 10);
    serial_putchar(COM1, '0' + p[2] % 10);
    serial_putchar(COM1, '.');
    serial_putchar(COM1, '0' + p[1] / 100);
    serial_putchar(COM1, '0' + (p[1] / 10) % 10);
    serial_putchar(COM1, '0' + p[1] % 10);
    serial_putchar(COM1, '.');
    serial_putchar(COM1, '0' + p[0] / 100);
    serial_putchar(COM1, '0' + (p[0] / 10) % 10);
    serial_putchar(COM1, '0' + p[0] % 10);
    serial_puts(COM1, " seq=");
    serial_putchar(COM1, '0' + seq / 100);
    serial_putchar(COM1, '0' + (seq / 10) % 10);
    serial_putchar(COM1, '0' + seq % 10);
    serial_puts(COM1, "\n");

    if (src_ip != ping_target) return;
    if (seq >= ping_sent) return;               // out of range
    if (!ping_received[seq]) {
        ping_received[seq] = true;
        ping_replies++;
        vga_puts("Reply from ");
        vga_print_ip(src_ip);
        vga_puts(" seq=");
        vga_print_dec(seq);
        vga_puts(" time=???\n");
    }
}

static void shell_ping(char* arg)
{
    serial_puts(COM1, "shell_ping called\n");
    uint32_t ip = parse_ip(arg);
    ping_target = ip;
    ping_replies = 0;
    ping_sent = 0;
    vga_puts("PING ");
    vga_print_ip(ip);
    vga_puts(" 56 data bytes\n");
    for (int i = 0; i < ping_total; i++) ping_received[i] = false;
    for (int i = 0; i < ping_total; i++) {
        uint8_t packet[64];
        memset(packet, 0, 64);
        
        // ICMP Echo Request Header
        packet[0] = 8; // Type: 8 (Echo Request)
        packet[1] = 0; // Code: 0
        packet[2] = 0; // Checksum (calculate later)
        packet[3] = 0; // Checksum
        packet[4] = 0; // Identifier
        packet[5] = 0; // Identifier
        packet[6] = (ping_sent >> 8) & 0xFF; // Sequence Number
        packet[7] = ping_sent & 0xFF;        // Sequence Number
        
        // Dummy Payload
        for (int j = 8; j < 64; j++) {
            packet[j] = j;
        }

        // Calculate and insert Checksum
        uint16_t csum = icmp_checksum(packet, 64);
        packet[2] = csum & 0xFF;
        packet[3] = (csum >> 8) & 0xFF;

        ipv4_send(ip, IP_PROTO_ICMP, packet, 64);
        ping_sent++;
        for (volatile int w = 0; w < 500000; w++);
    }

    // Wait for replies
    for (volatile int w = 0; w < 10000000; w++) {
        // Poll NIC to receive packets
        rtl8139_poll_receive();
    }
    
    uint32_t lost = (ping_sent > ping_replies) ? (ping_sent - ping_replies) : 0;
    uint32_t loss_percent = (ping_sent > 0) ? (lost * 100 / ping_sent) : 0;
    
    vga_puts("--- ");
    vga_print_ip(ip);
    vga_puts(" ping statistics ---\n");
    vga_print_dec(ping_sent);
    vga_puts(" packets transmitted, ");
    vga_print_dec(ping_replies);
    vga_puts(" received, ");
    vga_print_dec(loss_percent);
    vga_puts("% packet loss\n");
    
    serial_puts(COM1, "ping_sent=");
    serial_putchar(COM1, '0' + ping_sent);
    serial_puts(COM1, " ping_replies=");
    serial_putchar(COM1, '0' + ping_replies);
    serial_puts(COM1, "\n");
}

static void shell_arp(char* arg)
{
    uint32_t ip = parse_ip(arg);
    arp_request(ip);
    vga_puts("ARP request sent.\n");
}

static void shell_tcp(char* arg)
{
    if (strncmp(arg, "listen", 6) == 0) {
        char* port_str = arg + 7;
        while (*port_str == ' ') port_str++;
        uint16_t port = 0;
        while (*port_str >= '0' && *port_str <= '9') {
            port = port * 10 + (*port_str - '0');
            port_str++;
        }
        tcp_listen(port);
        vga_puts("Listening on port ");
        vga_print_dec(port);
        vga_putchar('\n');
    } else {
        vga_puts("Usage: tcp listen <port>\n");
    }
}

static void shell_ip(char* arg)
{
    serial_puts(COM1, "shell_ip: entered\n");
    while (*arg == ' ') arg++;
    if (*arg == '\0') {
        netif_t* iface = netif_get_default();
        vga_puts("IP address: ");
        vga_print_ip(iface->ipaddr);
        vga_putchar('\n');
    } else {
        uint32_t new_ip = parse_ip(arg);
        netif_set_ip(new_ip);
        vga_puts("IP set to ");
        vga_print_ip(new_ip);
        vga_putchar('\n');
    }
    serial_puts(COM1, "shell_ip: done\n");
}

static void shell_help(void)
{
    vga_puts("Commands:\n");
    vga_puts("  help            - show this help\n");
    vga_puts("  ip [address]    - show or set IP address\n");
    vga_puts("  arp <ip>        - send ARP request\n");
    vga_puts("  ping <ip>       - send ICMP echo requests\n");
    vga_puts("  tcp listen <port> - listen on TCP port\n");
}

void shell_run(void)
{
    network_register_icmp_reply_callback(icmp_reply_callback);

    char cmd[64];
    int idx = 0;
    vga_puts("MyKernel Shell\n> ");
    
    while (1) {
        // Poll NIC for packets even when no key
        rtl8139_poll_receive();

        if (keyboard_haschar()) {
            char c = keyboard_getchar();
            if (c == '\n') {
                cmd[idx] = '\0';
                vga_putchar('\n');
                
                if (strcmp(cmd, "help") == 0) {
                    shell_help();
                } else if (strncmp(cmd, "ip", 2) == 0) {
                    if (cmd[2] == ' ' || cmd[2] == '\0')
                        shell_ip(cmd + 2);
                    else
                        vga_puts("Unknown command.\n");
                } else if (strncmp(cmd, "arp ", 4) == 0) {
                    shell_arp(cmd + 4);
                } else if (strncmp(cmd, "ping ", 5) == 0) {
                    shell_ping(cmd + 5);
                } else if (strncmp(cmd, "tcp ", 4) == 0) {
                    shell_tcp(cmd + 4);
                } else if (cmd[0] != '\0') {
                    vga_puts("Unknown command. Type 'help'.\n");
                }
                
                vga_puts("> ");
                idx = 0;
                
            } else if (c == '\b') {
                if (idx > 0) {
                    idx--;
                    vga_putchar('\b');
                }
            } else {
                // Prevent buffer overflow while typing
                if (idx < 63) {
                    vga_putchar(c);
                    cmd[idx++] = c;
                }
            }
        }
    }
}
