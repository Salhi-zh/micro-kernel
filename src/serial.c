// src/serial.c
#include <serial.h>
#include <kernel.h>
static uint16_t debug_port = COM1;

void serial_init(uint16_t port)
{
    debug_port = port;
    outb(port + 1, 0x00);    /* Disable interrupts */
    outb(port + 3, 0x80);    /* Enable DLAB (baud divisor) */
    outb(port + 0, 0x03);    /* Divisor low (38400 baud) */
    outb(port + 1, 0x00);    /* Divisor high */
    outb(port + 3, 0x03);    /* 8 bits, no parity, one stop */
    outb(port + 2, 0xC7);    /* FIFO, clear, 14-byte threshold */
    outb(port + 4, 0x0B);    /* IRQs enabled, RTS/DSR set */
}

static int is_transmit_empty(uint16_t port)
{
    return inb(port + 5) & 0x20;
}

void serial_putchar(uint16_t port, char c)
{
    while (!is_transmit_empty(port)) { hlt(); }
    outb(port, c);
}

void serial_puts(uint16_t port, const char* str)
{
    while (*str) serial_putchar(port, *str++);
}
