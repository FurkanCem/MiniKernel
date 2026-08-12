#ifndef KERNEL_KEYBOARD_H
#define KERNEL_KEYBOARD_H

void keyboard_driver_init(void);

int kbd_read_char(char *out);
char kbd_getchar(void);

#endif
