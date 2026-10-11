
#include <kernel.h>
#include <gdt.h>
#include <idt.h>
#include <isr.h>
#include <irq.h>
#include <memory.h>
#include <vga.h>
#include <keyboard.h>
#include <serial.h>
#include <netif.h>
#include <network.h>
#include <shell.h>
#include <pci.h>
#include <rtl8139.h>
#include <multiboot.h>
// Helper for early serial output (before serial_init)
static void early_putc(char c)
{
    while (!(inb(COM1 + 5) & 0x20));
    outb(COM1, c);
}

void kernel_main(uint32_t magic, uint32_t addr)
{
    (void)magic;

    // Initialise COM1 manually for early debug
    outb(COM1 + 1, 0x00);
    outb(COM1 + 3, 0x80);
    outb(COM1 + 0, 0x03);
    outb(COM1 + 1, 0x00);
    outb(COM1 + 3, 0x03);
    outb(COM1 + 2, 0xC7);
    outb(COM1 + 4, 0x0B);

    early_putc('K');

    // Direct VGA writes (still visible because of explicit attribute)
    uint16_t* vga = (uint16_t*)0xB8000;
    vga[0] = 0x0F41;   // 'A'
    early_putc('A');

    gdt_init();
    early_putc('B');

    idt_init();
    early_putc('C');

    isr_init();
    early_putc('D');

    irq_init();
    early_putc('E');

    serial_init(COM1);
    serial_putchar(COM1, 'F');

    physmem_init((multiboot_info_t*)addr);
    serial_putchar(COM1, 'G');

    paging_init();
    serial_putchar(COM1, 'H');

    vga_clear();
    serial_putchar(COM1, 'I');

    // Set a visible colour (white on black)
    vga_setcolor(VGA_COLOR_LIGHT_GREY);

    // Test pattern (now using the default colour)
    for (int i = 0; i < 80; i++) {
        vga[i] = 0x0F20;               // white spaces (explicit colour for test)
    }
    vga[0] = 0x0F41;                   // 'A'
    vga[1] = 0x0F42;                   // 'B'
    vga[2] = 0x0F43;                   // 'C'
    vga[79] = 0x0F58;                  // 'X'

    // Now normal VGA functions will use the set colour
    vga_puts("MyKernel v0.1 – Booted successfully.\n");
    vga_puts("Type 'help' for commands.\n> ");

    keyboard_init();
    outb(0x21, inb(0x21) & ~0x02);
    sti();               // Enable interrupts for keyboard

    
    pci_init();
    pci_dev_t* rtl = pci_find_device(0x10EC, 0x8139); // Realtek vendor ID, RTL8139 device ID
    if (rtl) {
        rtl8139_init(rtl);
        // Replace default netif with RTL8139
        netif_t* rtl_iface = rtl8139_get_netif();
        // You may want to set IP via DHCP or static
        rtl_iface->ipaddr = 0x0A000002; // 10.0.0.2
    } else {
        serial_puts(COM1, "RTL8139 not found, using dummy netif\n");
    }
    shell_run();
}
