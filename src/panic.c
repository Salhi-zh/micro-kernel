#include <kernel.h>
#include <vga.h>

void kernel_panic(const char* msg)
{
    /* Use only direct VGA writes – no function calls that might fault */
    uint16_t* vga = (uint16_t*)0xB8000;
    const char* p = msg;
    int pos = 0;
    while (*p && pos < 80*25) {
        vga[pos++] = (uint16_t)*p++ | 0x0F00;  // white on black
    }

    cli();
    for (;;) {
        __asm__ volatile ("hlt");
    }
}
