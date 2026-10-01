#include <addresses.h>
#include <kprintf.h>
#include <mm.h>
#include <mm/frame.h>
#include <arch/arch.h>
#include <asm/cpu.h>
#include <asm/irq_flags.h>
#include <platform/platform.h>
#include <driver/timer.h>
#include <sched/thread.h>
#include <sched/scheduler.h>

#include "selftest/mm.h"
#include "selftest/scheduler.h"

static thread_t boot_thread;

/**
 * Kernel main entry point.
 *
 * @param address Physical address of the multiboot information structure.
 */
void kmain(physaddr_t address)
{
    struct multiboot_info *info = (struct multiboot_info *)phys_to_kern(address);

    kprintf("MiHOS\n");

    arch_init();

    platform_init();

    mmap_init(info);

    scheduler_init(&boot_thread);

    timer_init(TIMER_FREQUENCY_HZ);

    irq_enable();

    mm_selftest();
    scheduler_selftest();

    cpu_breakpoint();

    for (;;)
        cpu_idle();
}
