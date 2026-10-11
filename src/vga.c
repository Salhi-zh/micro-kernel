#include <vga.h>

static uint16_t* const VGA_MEMORY = (uint16_t*)0xB8000;
static const size_t VGA_WIDTH = 80;
static const size_t VGA_HEIGHT = 25;
static size_t row, col;
static uint8_t term_color;

static inline uint16_t vga_entry(char c, uint8_t color)
{
    return (uint16_t)c | (uint16_t)color << 8;
}

void vga_clear(void)
{
    for (size_t y = 0; y < VGA_HEIGHT; y++) {
        for (size_t x = 0; x < VGA_WIDTH; x++) {
            VGA_MEMORY[y * VGA_WIDTH + x] = vga_entry(' ', term_color);
        }
    }
    row = col = 0;
}

void vga_setcolor(uint8_t color)
{
    term_color = color;
}

void vga_putchar(char c)
{
    if (c == '\n') {
        col = 0;
        row++;
    } else if (c == '\b') {
        if (col > 0) {
            col--;
        } else if (row > 0) {
            row--;
            col = VGA_WIDTH - 1;
        } else {
            // At top-left, nothing to delete
            return;
        }
        // Erase character at new cursor position
        VGA_MEMORY[row * VGA_WIDTH + col] = vga_entry(' ', term_color);
    } else {
        VGA_MEMORY[row * VGA_WIDTH + col] = vga_entry(c, term_color);
        if (++col == VGA_WIDTH) {
            col = 0;
            row++;
        }
    }

    // Scroll if needed
    if (row == VGA_HEIGHT) {
        // Move all lines up
        for (size_t y = 0; y < VGA_HEIGHT - 1; y++) {
            for (size_t x = 0; x < VGA_WIDTH; x++) {
                VGA_MEMORY[y * VGA_WIDTH + x] = VGA_MEMORY[(y + 1) * VGA_WIDTH + x];
            }
        }
        // Clear last line
        for (size_t x = 0; x < VGA_WIDTH; x++) {
            VGA_MEMORY[(VGA_HEIGHT - 1) * VGA_WIDTH + x] = vga_entry(' ', term_color);
        }
        row = VGA_HEIGHT - 1;
    }
}

void vga_puts(const char* str)
{
    while (*str) vga_putchar(*str++);
}
