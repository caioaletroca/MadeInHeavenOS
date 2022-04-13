#ifndef _KERNEL_TTY_H_
#define _KERNEL_TTY_H_ 1

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <driver/screen.h>

size_t tty_write(const void *str, size_t n);
void tty_init();

// void terminal_clear(void);
// void terminal_put(char c);
// void terminal_write(const char *data, size_t size);
// void terminal_writestring(const char *data);

#endif