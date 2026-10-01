#include "panic.h"
#include <asm/cpu.h>
#include <asm/irq_flags.h>

__attribute__((noreturn, format(printf, 1, 2))) void panic(const char *fmt, ...)
{
    va_list args;

    irq_disable();

    va_start(args, fmt);
    kvprintf(fmt, args);
    va_end(args);

    cpu_halt();
}
