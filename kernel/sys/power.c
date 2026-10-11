#include <power.h>
#include <asm/irq_flags.h>
#include <asm/cpu.h>
#include <platform/platform.h>
#include <kprintf.h>

__attribute__((noreturn)) void system_halt(void)
{
    irq_disable();
    kprintf("system halted\n");
    platform_power_off();
    cpu_halt();
}