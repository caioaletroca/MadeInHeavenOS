#include <addresses.h>
#include <kprintf.h>
#include <interrupts.h>
#include <mm.h>
#include <paging.h>
#include "idt.h"
#include "driver/ps2.h"

#include <mm/slab.h>

void kmain(physaddr_t address)
{
    struct multiboot_info *info = (struct multiboot_info *)phys_to_kern(address);

    kprintf("MiHOS\n");

    interrupts_init();

    ps2_init();

    enable_interrupts();

    mmap_init(info);

    paging_init();

    slab_cache_t cache;
    if (slab_cache_init(&cache, "example_cache", sizeof(uint64_t), 0) != 0)
        panic("Failed to initialize slab cache\n");

    uint64_t *value = slab_cache_alloc(&cache);

    if (value == NULL)
        panic("Failed to allocate from slab cache\n");

    *value = 42;
    kprintf("Allocated value: %u\n", *value);

    if (slab_cache_free(&cache, value) != 0)
    {
        panic("Failed to free object back to slab cache\n");
    }

    kprintf("Finished slab cache test\n");

    // Magic breakpoint
    __asm__ __volatile__("xchgw %bx, %bx");

    while (1)
    {
        __asm__ __volatile__("hlt");
    }
}