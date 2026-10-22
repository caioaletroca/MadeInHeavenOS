#include <power.h>
#include <asm/irq_flags.h>
#include <asm/cpu.h>
#include <driver/block_cache.h>
#include <platform/platform.h>
#include <kprintf.h>

__attribute__((noreturn)) void system_halt(void)
{
    int ret = block_cache_sync();
    if (ret < 0)
        kprintf("system_halt: writing the disk cache failed: %d\n", ret);

    irq_disable();
    kprintf("system halted\n");
    platform_power_off();
    cpu_halt();
}