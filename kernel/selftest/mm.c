#include "selftest/mm.h"
#include <addresses.h>
#include <assert.h>
#include <panic.h>
#include <arch/mmu.h>
#include <asm/memory.h>
#include <mm/frame.h>
#include <kprintf.h>

/**
 * Tests the mapping and unmapping of a single page in the page table.
 */
static void mm_page_map_unmap_test(void)
{
    mmu_root_t *root = arch_mmu_kernel_root();

    KASSERT(root != NULL);

    physaddr_t physical = (physaddr_t)frame_alloc(0, 0);
    if (physical == 0)
        panic("Failed to allocate paging test frame\n");

    uintptr_t virtual = KERNEL_SELFTEST_VIRTUAL_BASE;

    if (arch_mmu_map(root, virtual, physical, MMU_WRITE) != 0)
        panic("Failed to map paging test page\n");

    volatile uint64_t *mapped = (volatile uint64_t *)virtual;
    volatile uint64_t *direct = (volatile uint64_t *)phys_to_kern(physical);

    *mapped = 0x123456789ABCDEF0ULL;

    if (*direct != 0x123456789ABCDEF0ULL)
        panic("Mapped page does not alias physical frame\n");

    if (arch_mmu_unmap(root, virtual) != 0)
        panic("Failed to unmap paging test page\n");

    frame_free((void *)physical, 0);
}

/**
 * Tests the translation of virtual addresses to physical addresses and ensures
 * that the mappings are correctly established and removed.
 */
static void mm_page_translate_test(void)
{
    mmu_root_t *root = arch_mmu_kernel_root();
    KASSERT(root != NULL);

    physaddr_t physical_a = (physaddr_t)frame_alloc(0, 0);
    physaddr_t physical_b = (physaddr_t)frame_alloc(0, 0);

    uintptr_t virtual_a = KERNEL_SELFTEST_VIRTUAL_BASE;
    uintptr_t virtual_b = virtual_a + PAGE_SIZE;

    KASSERT(physical_a != 0);
    KASSERT(physical_b != 0);
    KASSERT(physical_a != physical_b);

    // Map the virtual addresses to the allocated physical frames
    KASSERT(arch_mmu_map(root, virtual_a, physical_a, MMU_WRITE) == 0);
    KASSERT(arch_mmu_map(root, virtual_b, physical_b, MMU_WRITE) == 0);

    physaddr_t translated_a;
    physaddr_t translated_b;

    // Translate the virtual addresses back to physical addresses
    KASSERT(arch_mmu_translate(root, virtual_a, &translated_a) == 0);
    KASSERT(arch_mmu_translate(root, virtual_b, &translated_b) == 0);

    KASSERT(translated_a == physical_a);
    KASSERT(translated_b == physical_b);

    KASSERT(arch_mmu_translate(root, virtual_a + 123, &translated_a) == 0);
    KASSERT(translated_a == physical_a + 123);

    // Unmap the virtual addresses and verify they are no longer mapped
    KASSERT(arch_mmu_unmap(root, virtual_a) == 0);

    // Verify that virtual_a is no longer mapped
    KASSERT(arch_mmu_translate(root, virtual_a, &translated_a) != 0);
    KASSERT(arch_mmu_translate(root, virtual_b, &translated_b) == 0);

    // Unmap virtual_b and verify it is no longer mapped
    KASSERT(arch_mmu_unmap(root, virtual_b) == 0);

    // Verify that virtual_b is no longer mapped
    KASSERT(arch_mmu_translate(root, virtual_b, &translated_b) != 0);

    frame_free((void *)physical_a, 0);
    frame_free((void *)physical_b, 0);
}

void mm_selftest(void)
{
    kprintf("Running memory management self-test...\n");

    mm_page_map_unmap_test();

    mm_page_translate_test();

    kprintf("Paging map/unmap test passed\n");
}