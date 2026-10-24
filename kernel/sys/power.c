#include <power.h>
#include <asm/irq_flags.h>
#include <asm/cpu.h>
#include <driver/block_cache.h>
#include <platform/platform.h>
#include <kprintf.h>

/**
 * List of registered halt hooks.
 */
static list_t halt_hooks = {&halt_hooks, &halt_hooks};

void halt_hook_add(halt_hook_t *hook)
{
    list_insert_after(&halt_hooks, &hook->link);
}

__attribute__((noreturn)) void system_halt(void)
{
    // Run all registered halt hooks before halting the system
    list_t *pos;
    list_for_each(pos, &halt_hooks)
    {
        halt_hook_t *hook = list_container(pos, halt_hook_t, link);
        hook->run(hook);
    }

    int ret = block_cache_sync();
    if (ret < 0)
        kprintf("system_halt: writing the disk cache failed: %d\n", ret);

    irq_disable();
    kprintf("system halted\n");
    platform_power_off();
    cpu_halt();
}