#include <arch/arch.h>
#include <asm/cpu.h>
#include <asm/irq_flags.h>
#include <addresses.h>
#include <mm/mm.h>
#include <driver/input.h>
#include <driver/tty.h>
#include <driver/timer.h>
#include <driver/console.h>
#include <driver/mem.h>
#include <driver/block.h>
#include <driver/block_cache.h>
#include <driver/partition.h>
#include <platform/platform.h>
#include <sched/thread.h>
#include <sched/scheduler.h>
#include <fs/devfs.h>
#include <fs/ext2.h>
#include <fs/root.h>
#include <kprintf.h>
#include <kernel_args.h>
#include <boot_info.h>
#include <init.h>

#include "selftest/block.h"
#include "selftest/console.h"
#include "selftest/elf.h"
#include "selftest/ext2.h"
#include "selftest/guard.h"
#include "selftest/hashtable.h"
#include "selftest/mm.h"
#include "selftest/partition.h"
#include "selftest/scheduler.h"
#include "selftest/user.h"
#include "selftest/vfs.h"

static thread_t boot_thread;
static boot_info_t boot_info;
static kernel_args_t kernel_args;

/**
 * Kernel main entry point.
 *
 * @param boot_handoff The boot handoff information passed to the kernel.
 */
void kmain(uintptr_t boot_handoff)
{
    tty_init();

    platform_boot_info_init(boot_handoff, &boot_info);
    kernel_args_parse(boot_info.cmdline, &kernel_args);

    kprintf("MiHOS\n");

    arch_init();

    input_init();

    platform_init();

    mm_init(&boot_info);

    devfs_init();
    mem_devices_init();

    scheduler_init(&boot_thread);
    threads_init();
    console_init(&keymap_abnt2);

    block_cache_init();

    timer_init(TIMER_FREQUENCY_HZ);

    irq_enable();
    partition_init();
    block_devfs_init();

    root_init(&kernel_args, &boot_info);
    devfs_mount();
    console_open();
    ext2_init();

    guard_selftest();
    hashtable_selftest();
    mm_selftest();
    scheduler_selftest();
    console_selftest();
    user_selftest();
    vfs_selftest();
    block_selftest();
    partition_selftest();
    ext2_selftest(); // after the block one: that one counts cache hits and misses
    elf_selftest();
    kprintf("self-tests: ok\n"); // each one panics on failure

    init_start(kernel_args.init_args);

    for (;;)
        cpu_idle();
}
