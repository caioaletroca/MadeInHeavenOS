#include <kprintf.h>

void kvprintf(const char *fmt, va_list args) {
    tty_write(fmt, sizeof(fmt));
}

void kprintf(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    kvprintf(fmt, ap);
    va_end(ap);
}