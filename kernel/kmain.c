#include <arch/arch.h>
#include <asm/cpu.h>
#include <asm/irq_flags.h>
#include <addresses.h>
#include <kprintf.h>
#include <mm.h>
#include <driver/input.h>
#include <platform/platform.h>
#include <driver/timer.h>
#include <sched/thread.h>
#include <sched/scheduler.h>
#include <boot_info.h>

#include "selftest/mm.h"
#include "selftest/scheduler.h"

static thread_t boot_thread;
static boot_info_t boot_info;

static void keyboard_reader(void *arg)
{
    (void)arg;

    for (;;)
    {
        input_event_t event = input_get_event();
        kprintf("Key=%u %s\n", event.key, event.pressed ? "pressed" : "released");
    }
}

/**
 * Kernel main entry point.
 *
 * @param boot_handoff The boot handoff information passed to the kernel.
 */
void kmain(uintptr_t boot_handoff)
{
    platform_boot_info_init(boot_handoff, &boot_info);

    kprintf("MiHOS\n");

    arch_init();

    input_init();

    platform_init();

    mm_init(&boot_info);

    scheduler_init(&boot_thread);
    threads_init();
    thread_create(keyboard_reader, NULL);

    timer_init(TIMER_FREQUENCY_HZ);

    irq_enable();

    mm_selftest();
    scheduler_selftest();

    cpu_breakpoint();

    for (;;)
        cpu_idle();
}
