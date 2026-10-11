// include/keyboard.h
#ifndef KEYBOARD_H
#define KEYBOARD_H

#include <kernel.h>

void keyboard_init(void);
char keyboard_getchar(void);
bool keyboard_haschar(void);

#endif
