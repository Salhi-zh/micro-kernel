#include <idt.h>

/* Declare the IDT and pointer */
static idt_entry_t idt_entries[256];
static idt_ptr_t   idt_ptr;

/* Set one IDT entry */
void idt_set_gate(uint8_t num, uint32_t base, uint16_t selector, uint8_t flags)
{
    idt_entries[num].base_low  = base & 0xFFFF;
    idt_entries[num].base_high = (base >> 16) & 0xFFFF;
    idt_entries[num].selector   = selector;
    idt_entries[num].zero       = 0;
    idt_entries[num].flags      = flags;
}

/* Initialise the IDT */
void idt_init(void)
{
    idt_ptr.limit = sizeof(idt_entries) - 1;
    idt_ptr.base  = (uint32_t)&idt_entries;

    /* Clear all entries (optional) */
    for (int i = 0; i < 256; i++) {
        idt_set_gate(i, 0, 0, 0);
    }

    /* Load IDT */
    __asm__ volatile ("lidt %0" : : "m"(idt_ptr));
}
