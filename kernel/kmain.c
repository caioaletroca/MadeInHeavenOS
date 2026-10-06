#include <arch/arch.h>
#include <asm/cpu.h>
#include <asm/irq_flags.h>
#include <addresses.h>
#include <kprintf.h>
#include <mm/mm.h>
#include <driver/input.h>
#include <driver/tty.h>
#include <driver/timer.h>
#include <driver/console.h>
#include <platform/platform.h>
#include <sched/thread.h>
#include <sched/scheduler.h>
#include <boot_info.h>

#include "selftest/guard.h"
#include "selftest/mm.h"
#include "selftest/scheduler.h"
#include "selftest/user.h"

static thread_t boot_thread;
static boot_info_t boot_info;

/**
 * Kernel main entry point.
 *
 * @param boot_handoff The boot handoff information passed to the kernel.
 */
void kmain(uintptr_t boot_handoff)
{
    tty_init();

    platform_boot_info_init(boot_handoff, &boot_info);

    kprintf("MiHOS\n");

    arch_init();

    input_init();

    platform_init();

    mm_init(&boot_info);

    scheduler_init(&boot_thread);
    threads_init();
    console_init(&keymap_abnt2);

    timer_init(TIMER_FREQUENCY_HZ);

    irq_enable();

    guard_selftest();
    mm_selftest();
    scheduler_selftest();
    user_selftest();

    cpu_breakpoint();

    for (;;)
        cpu_idle();
}
