
#include <irq.h>
#include <idt.h>
#include <isr.h>
#include <kernel.h>
#include <serial.h>

static irq_handler_t irq_routines[16];

static void pic_remap(void)
{
    outb(0x20, 0x11);
    outb(0xA0, 0x11);
    outb(0x21, 0x20);
    outb(0xA1, 0x28);
    outb(0x21, 0x04);
    outb(0xA1, 0x02);
    outb(0x21, 0x01);
    outb(0xA1, 0x01);
    outb(0x21, 0xFF);
    outb(0xA1, 0xFF);
}

void irq_install_handler(int irq, irq_handler_t handler)
{
    if (irq >= 0 && irq < 16) {
        irq_routines[irq] = handler;
    }
}

void irq_uninstall_handler(int irq)
{
    if (irq >= 0 && irq < 16) {
        irq_routines[irq] = NULL;
    }
}

void irq_handler(registers_t* regs)
{
    int irq = regs->int_no - 32;
    if (irq >= 0 && irq < 16 && irq_routines[irq] != NULL) {
        irq_routines[irq](regs);
    }
    if (irq >= 8) {
        outb(0xA0, 0x20);
    }
    outb(0x20, 0x20);
}

void irq_init(void)
{
    pic_remap();

    idt_set_gate(32, (uint32_t)irq0, 0x08, 0x8E);
    idt_set_gate(33, (uint32_t)irq1, 0x08, 0x8E);
    idt_set_gate(34, (uint32_t)irq2, 0x08, 0x8E);
    idt_set_gate(35, (uint32_t)irq3, 0x08, 0x8E);
    idt_set_gate(36, (uint32_t)irq4, 0x08, 0x8E);
    idt_set_gate(37, (uint32_t)irq5, 0x08, 0x8E);
    idt_set_gate(38, (uint32_t)irq6, 0x08, 0x8E);
    idt_set_gate(39, (uint32_t)irq7, 0x08, 0x8E);
    idt_set_gate(40, (uint32_t)irq8, 0x08, 0x8E);
    idt_set_gate(41, (uint32_t)irq9, 0x08, 0x8E);
    idt_set_gate(42, (uint32_t)irq10, 0x08, 0x8E);
    idt_set_gate(43, (uint32_t)irq11, 0x08, 0x8E);
    idt_set_gate(44, (uint32_t)irq12, 0x08, 0x8E);
    idt_set_gate(45, (uint32_t)irq13, 0x08, 0x8E);
    idt_set_gate(46, (uint32_t)irq14, 0x08, 0x8E);
    idt_set_gate(47, (uint32_t)irq15, 0x08, 0x8E);

    for (int i = 0; i < 16; i++) {
        irq_routines[i] = NULL;
    }
}

void irq_unmask(int irq)
{
    if (irq < 8) {
        uint8_t mask = inb(0x21);
        serial_puts(COM1, "PIC master mask before: ");
        serial_putchar(COM1, "0123456789ABCDEF"[mask >> 4]);
        serial_putchar(COM1, "0123456789ABCDEF"[mask & 0xF]);
        serial_puts(COM1, "\n");
        outb(0x21, mask & ~(1 << irq));
        mask = inb(0x21);
        serial_puts(COM1, "PIC master mask after: ");
        serial_putchar(COM1, "0123456789ABCDEF"[mask >> 4]);
        serial_putchar(COM1, "0123456789ABCDEF"[mask & 0xF]);
        serial_puts(COM1, "\n");
    } else {
        uint8_t mask = inb(0xA1);
        serial_puts(COM1, "PIC slave mask before: ");
        serial_putchar(COM1, "0123456789ABCDEF"[mask >> 4]);
        serial_putchar(COM1, "0123456789ABCDEF"[mask & 0xF]);
        serial_puts(COM1, "\n");
        outb(0xA1, mask & ~(1 << (irq - 8)));
        mask = inb(0xA1);
        serial_puts(COM1, "PIC slave mask after: ");
        serial_putchar(COM1, "0123456789ABCDEF"[mask >> 4]);
        serial_putchar(COM1, "0123456789ABCDEF"[mask & 0xF]);
        serial_puts(COM1, "\n");
    }
}
