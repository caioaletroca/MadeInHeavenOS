#include "panic.h"

__attribute__((noreturn, format(printf, 1, 2))) void panic(const char *fmt, ...)
{
    va_list args;

    __asm__ __volatile__("cli");

    va_start(args, fmt);
    kvprintf(fmt, args);
    va_end(args);

    for (;;)
        __asm__ __volatile__("hlt");
}