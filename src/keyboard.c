
#include <keyboard.h>
#include <irq.h>
#include <kernel.h>

#define KEYBOARD_DATA_PORT 0x60
#define KEYBOARD_STATUS_PORT 0x64

static volatile char last_char = 0;
static volatile bool has_char = false;
static volatile bool shift_pressed = false;
static volatile bool ctrl_pressed = false;
static volatile bool alt_pressed = false;

// Unshifted AZERTY scancode set 1 (using CP437 codes)
static const unsigned char scancode_ascii_lower[128] = {
    0,    /* 0x00 - error */
    0,    /* 0x01 - escape */
    '&',  /* 0x02 - 1 */
    0x82, /* 0x03 - 2 : é */
    0x22, /* 0x04 - 3 : " */
    0x27, /* 0x05 - 4 : ' */
    '(',  /* 0x06 - 5 */
    '-',  /* 0x07 - 6 */
    0x8A, /* 0x08 - 7 : è */
    '_',  /* 0x09 - 8 */
    0x87, /* 0x0A - 9 : ç */
    0x85, /* 0x0B - 0 : à */
    ')',  /* 0x0C - °? actually ) */
    '=',  /* 0x0D - = */
    '\b', /* 0x0E - backspace */
    '\t', /* 0x0F - tab */
    'a',  /* 0x10 - a */
    'z',  /* 0x11 - z */
    'e',  /* 0x12 - e */
    'r',  /* 0x13 - r */
    't',  /* 0x14 - t */
    'y',  /* 0x15 - y */
    'u',  /* 0x16 - u */
    'i',  /* 0x17 - i */
    'o',  /* 0x18 - o */
    'p',  /* 0x19 - p */
    '^',  /* 0x1A - dead circumflex */
    '$',  /* 0x1B - $ */
    '\n', /* 0x1C - enter */
    0,    /* 0x1D - left ctrl */
    'q',  /* 0x1E - q */
    's',  /* 0x1F - s */
    'd',  /* 0x20 - d */
    'f',  /* 0x21 - f */
    'g',  /* 0x22 - g */
    'h',  /* 0x23 - h */
    'j',  /* 0x24 - j */
    'k',  /* 0x25 - k */
    'l',  /* 0x26 - l */
    'm',  /* 0x27 - m */
    0x97, /* 0x28 - ù */
    '`',  /* 0x29 - dead grave */
    0,    /* 0x2A - left shift */
    '*',  /* 0x2B - keypad * */
    'w',  /* 0x2C - w */
    'x',  /* 0x2D - x */
    'c',  /* 0x2E - c */
    'v',  /* 0x2F - v */
    'b',  /* 0x30 - b */
    'n',  /* 0x31 - n */
    ',',  /* 0x32 - , */
    ';',  /* 0x33 - ; */
    ':',  /* 0x34 - : */
    '!',  /* 0x35 - ! */
    0,    /* 0x36 - right shift */
    '*',  /* 0x37 - keypad * (again) */
    0,    /* 0x38 - alt */
    ' ',  /* 0x39 - space */
    0,    /* 0x3A - caps lock */
    0,    /* 0x3B - F1 */
    0,    /* 0x3C - F2 */
    0,    /* 0x3D - F3 */
    0,    /* 0x3E - F4 */
    0,    /* 0x3F - F5 */
    0,    /* 0x40 - F6 */
    0,    /* 0x41 - F7 */
    0,    /* 0x42 - F8 */
    0,    /* 0x43 - F9 */
    0,    /* 0x44 - F10 */
    0,    /* 0x45 - num lock */
    0,    /* 0x46 - scroll lock */
    0,    /* 0x47 - home */
    0,    /* 0x48 - up */
    0,    /* 0x49 - page up */
    0,    /* 0x4A - keypad - */
    0,    /* 0x4B - left */
    0,    /* 0x4C - keypad 5? */
    0,    /* 0x4D - right */
    0,    /* 0x4E - keypad + */
    0,    /* 0x4F - end */
    0,    /* 0x50 - down */
    0,    /* 0x51 - page down */
    0,    /* 0x52 - insert */
    0,    /* 0x53 - delete */
    0,    /* 0x54 - F11? */
    0,    /* 0x55 - F12? */
    0,    /* 0x56 - ? */
    0     /* 0x57 - ? */
};

// Shifted AZERTY scancode set 1 (using CP437 codes)
static const unsigned char scancode_ascii_upper[128] = {
    0,    /* 0x00 */
    0,    /* 0x01 */
    '1',  /* 0x02 - 1 shifted = 1 */
    '2',  /* 0x03 - 2 shifted = 2 */
    '3',  /* 0x04 - 3 shifted = 3 */
    '4',  /* 0x05 - 4 shifted = 4 */
    '5',  /* 0x06 - 5 shifted = 5 */
    '6',  /* 0x07 - 6 shifted = 6 */
    '7',  /* 0x08 - 7 shifted = 7 */
    '8',  /* 0x09 - 8 shifted = 8 */
    '9',  /* 0x0A - 9 shifted = 9 */
    '0',  /* 0x0B - 0 shifted = 0 */
    ']',  /* 0x0C - ) becomes ]? Actually on AZERTY, shift+) gives °? Let's use ')'? We'll keep as ')' for now */
    '+',  /* 0x0D - = becomes + */
    '\b', /* 0x0E - backspace */
    '\t', /* 0x0F - tab */
    'A',  /* 0x10 - A */
    'Z',  /* 0x11 - Z */
    'E',  /* 0x12 - E */
    'R',  /* 0x13 - R */
    'T',  /* 0x14 - T */
    'Y',  /* 0x15 - Y */
    'U',  /* 0x16 - U */
    'I',  /* 0x17 - I */
    'O',  /* 0x18 - O */
    'P',  /* 0x19 - P */
    '^',  /* 0x1A - dead circumflex (shift same?) */
    '$',  /* 0x1B - $ (shift same?) */
    '\n', /* 0x1C - enter */
    0,    /* 0x1D - left ctrl */
    'Q',  /* 0x1E - Q */
    'S',  /* 0x1F - S */
    'D',  /* 0x20 - D */
    'F',  /* 0x21 - F */
    'G',  /* 0x22 - G */
    'H',  /* 0x23 - H */
    'J',  /* 0x24 - J */
    'K',  /* 0x25 - K */
    'L',  /* 0x26 - L */
    'M',  /* 0x27 - M */
    0x99, /* 0x28 - Ù (capital) – CP437 0x99 = Ù */
    '~',  /* 0x29 - dead grave becomes ~? Actually shift+grave gives ~? Let's use '~' */
    0,    /* 0x2A - left shift (release handled) */
    '*',  /* 0x2B - keypad * */
    'W',  /* 0x2C - W */
    'X',  /* 0x2D - X */
    'C',  /* 0x2E - C */
    'V',  /* 0x2F - V */
    'B',  /* 0x30 - B */
    'N',  /* 0x31 - N */
    '?',  /* 0x32 - , becomes ? */
    '.',  /* 0x33 - ; becomes . (dot) */
    '/',  /* 0x34 - : becomes / */
    0xAD, /* 0x35 - ! becomes ¿? Actually shift+! is ? maybe? We'll use '¡' but CP437 0xAD is ¡? Let's use '!' unchanged for simplicity */
    0,    /* 0x36 - right shift */
    '*',  /* 0x37 - keypad * */
    0,    /* 0x38 - alt */
    ' ',  /* 0x39 - space */
    0,    /* 0x3A - caps lock */
    0,    /* ... */
};

static void keyboard_handler(registers_t* regs)
{
    (void)regs;
    uint8_t scancode = inb(KEYBOARD_DATA_PORT);

    // Handle key release (scancode with 0x80 set)
    if (scancode & 0x80) {
        uint8_t release = scancode & 0x7F;
        if (release == 0x2A || release == 0x36) {
            shift_pressed = false;
        } else if (release == 0x1D) {
            ctrl_pressed = false;
        } else if (release == 0x38) {
            alt_pressed = false;
        }
        return;
    }

    // Key press
    if (scancode < 128) {
        // Update modifier state
        if (scancode == 0x2A || scancode == 0x36) {
            shift_pressed = true;
            return;
        } else if (scancode == 0x1D) {
            ctrl_pressed = true;
            return;
        } else if (scancode == 0x38) {
            alt_pressed = true;
            return;
        }

        // Get character based on shift state
        unsigned char c;
        if (shift_pressed) {
            c = scancode_ascii_upper[scancode];
        } else {
            c = scancode_ascii_lower[scancode];
        }

        if (c) {
            last_char = (char)c;
            has_char = true;
        }
    }
}

void keyboard_init(void)
{
    irq_install_handler(1, keyboard_handler);
    shift_pressed = false;
    ctrl_pressed = false;
    alt_pressed = false;
}

char keyboard_getchar(void)
{
    while (!has_char) { hlt(); }
    has_char = false;
    return last_char;
}

bool keyboard_haschar(void)
{
    return has_char;
}
