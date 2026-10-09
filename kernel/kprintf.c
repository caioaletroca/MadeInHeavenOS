#include <kprintf.h>

#define KPRINTF_BUFFER_SIZE 512

void kvprintf(const char *fmt, va_list args)
{
    char str[KPRINTF_BUFFER_SIZE];

    // Formats string
    int n = vsnprintf(str, KPRINTF_BUFFER_SIZE, fmt, args);

    // n is the full length: a longer message was cut to the buffer
    if (n >= KPRINTF_BUFFER_SIZE)
        n = KPRINTF_BUFFER_SIZE - 1;

    // Put into the console
    if (n > 0)
        tty_write(str, n);
}

void kprintf(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    kvprintf(fmt, args);
    va_end(args);
}