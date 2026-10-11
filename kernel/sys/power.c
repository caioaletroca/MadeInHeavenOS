#include <power.h>
#include <asm/irq_flags.h>
#include <asm/cpu.h>
#include <kprintf.h>

__attribute__((noreturn)) void system_halt(void)
{
    irq_disable();
    kprintf("system halted\n");
    cpu_halt();
}