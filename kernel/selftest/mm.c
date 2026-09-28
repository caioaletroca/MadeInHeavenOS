#include "selftest/mm.h"
#include <addresses.h>
#include <panic.h>
#include <paging.h>
#include <mm/frame.h>
#include <kprintf.h>

extern page_table_t page_table_l4;
void mm_selftest(void)
{
    kprintf("Running memory management self-test...\n");

    physaddr_t physical = (physaddr_t)frame_alloc(0, 0);
    if (physical == 0)
        panic("Failed to allocate paging test frame\n");

    uintptr_t virtual = 0xFFFF900000000000ULL;

    if (page_map(&page_table_l4, virtual, physical, PAGE_TABLE_ENTRY_WRITE) != 0)
        panic("Failed to map paging test page\n");

    volatile uint64_t *mapped = (volatile uint64_t *)virtual;
    volatile uint64_t *direct = (volatile uint64_t *)phys_to_kern(physical);

    *mapped = 0x123456789ABCDEF0ULL;

    if (*direct != 0x123456789ABCDEF0ULL)
        panic("Mapped page does not alias physical frame\n");

    if (page_unmap(&page_table_l4, virtual) != 0)
        panic("Failed to unmap paging test page\n");

    frame_free((void *)physical, 0);

    kprintf("Paging map/unmap test passed\n");
}