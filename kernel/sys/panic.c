#include "panic.h"

// TODO: Make this better
void panic(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    kvprintf(fmt, args);
    va_end(args);
    halt();
}