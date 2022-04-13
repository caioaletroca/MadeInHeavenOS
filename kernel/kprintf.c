#include <kprintf.h>

#define KPRINTF_BUFFER_SIZE 64

void kvprintf(const char *fmt, va_list args) {
    char str[KPRINTF_BUFFER_SIZE];
    
    // Formats string
    int n = vsnprintf(str, 64, fmt, args);

    // Put into the console
    if (n > 0)
        tty_write(str, n);
}

void kprintf(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    kvprintf(fmt, args);
    va_end(args);
}