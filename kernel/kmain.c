#include <addresses.h>
#include <kprintf.h>
#include <interrupts.h>
#include <mm.h>
#include <paging.h>
#include <mm/frame.h>
#include "idt.h"
#include "driver/ps2.h"
#include "driver/timer.h"
#include "selftest/mm.h"

/**
 * Kernel main entry point.
 *
 * @param address Physical address of the multiboot information structure.
 */
void kmain(physaddr_t address)
{
    struct multiboot_info *info = (struct multiboot_info *)phys_to_kern(address);

    kprintf("MiHOS\n");

    interrupts_init();

    ps2_init();
    timer_init(100);

    enable_interrupts();

    mmap_init(info);

    paging_init();

    mm_selftest();

    // Magic breakpoint
    __asm__ __volatile__("xchgw %bx, %bx");

    uint64_t last_seconds = 0;

    while (1)
    {
        __asm__ __volatile__("hlt");

        uint64_t seconds = timer_ticks() / 100;

        if (seconds != last_seconds)
        {
            last_seconds = seconds;
            kprintf("Timer: %u seconds\n", seconds);
        }
    }
}