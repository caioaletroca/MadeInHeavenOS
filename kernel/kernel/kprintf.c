#include <kernel/kprintf.h>

void kprintf(const char *str) {
    tty_putchar(str[0]);
}