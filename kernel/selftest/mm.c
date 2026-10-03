#include "selftest/mm.h"
#include <addresses.h>
#include <assert.h>
#include <arch/mmu.h>
#include <asm/memory.h>
#include <mm/frame.h>
#include <mm/kmalloc.h>
#include <mm/address_space.h>
#include <string.h>
#include <panic.h>
#include <kprintf.h>

/**
 * Tests the mapping and unmapping of a single page in the page table.
 */
static void mm_page_map_unmap_test(void)
{
    mmu_root_t *root = arch_mmu_kernel_root();

    KASSERT(root != NULL);

    physaddr_t physical = frame_alloc(0, 0);
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

    frame_free(physical, 0);
}

/**
 * Tests the translation of virtual addresses to physical addresses and ensures
 * that the mappings are correctly established and removed.
 */
static void mm_page_translate_test(void)
{
    mmu_root_t *root = arch_mmu_kernel_root();
    KASSERT(root != NULL);

    physaddr_t physical_a = frame_alloc(0, 0);
    physaddr_t physical_b = frame_alloc(0, 0);

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

    frame_free(physical_a, 0);
    frame_free(physical_b, 0);
}

/**
 * Tests the kernel memory allocator (kmalloc and kfree) for various allocation sizes,
 * alignment, reuse of freed objects, and large allocations.
 */
static void mm_kmalloc_test(void)
{
    // Every size class, written end to end
    for (size_t size = 1; size <= 4096 * 3; size = size * 2 + 1)
    {
        uint8_t *block = kmalloc(size);
        KASSERT(block != NULL);
        KASSERT(((uintptr_t)block & 15) == 0);

        for (size_t i = 0; i < size; i++)
            block[i] = (uint8_t)i;
        for (size_t i = 0; i < size; i++)
            KASSERT(block[i] == (uint8_t)i);

        kfree(block);
    }

    // Freed slab objects are reused
    void *first = kmalloc(64);
    kfree(first);
    void *second = kmalloc(64);
    KASSERT(first == second);
    kfree(second);

    // Many live objects span several slabs and stay distinct
    void *objects[200];
    for (size_t i = 0; i < 200; i++)
    {
        objects[i] = kmalloc(32);
        KASSERT(objects[i] != NULL);
        *(size_t *)objects[i] = i;
    }
    for (size_t i = 0; i < 200; i++)
    {
        KASSERT(*(size_t *)objects[i] == i);
        kfree(objects[i]);
    }

    // Large allocations are page aligned
    void *large = kmalloc(5000);
    KASSERT(large != NULL && ((uintptr_t)large & (PAGE_SIZE - 1)) == 0);
    kfree(large);

    kfree(NULL);
    KASSERT(kmalloc(0) == NULL);

    // Small sizes come from slab caches: never page aligned (header owns offset 0)
    void *small = kmalloc(16);
    KASSERT(small != NULL);
    KASSERT(((uintptr_t)small & (PAGE_SIZE - 1)) != 0);
    kfree(small);
}

/**
 * Tests that an address space maps user pages privately and leaves the
 * kernel half usable while it is active.
 */
static void mm_address_space_test(void)
{
    static const char message[] = "user half";
    physaddr_t physical;

    address_space_t *space = address_space_create();
    if (space == NULL)
        panic("Failed to create test address space\n");

    // Two pages, with the write straddling the boundary between them
    if (address_space_map(space, USER_BASE, 2 * PAGE_SIZE, MMU_WRITE) != 0)
        panic("Failed to map test user pages\n");

    uintptr_t target = USER_BASE + PAGE_SIZE - 4;

    if (address_space_write(space, target, message, sizeof(message)) != 0)
        panic("Failed to write test user pages\n");

    // Only the new space sees the user pages
    if (arch_mmu_translate(arch_mmu_kernel_root(), USER_BASE, &physical) == 0)
        panic("User page leaked into the kernel root\n");

    address_space_activate(space);

    // Kernel code keeps running (kernel half shared) and sees the user data
    if (memcmp((const void *)target, message, sizeof(message)) != 0)
        panic("User pages do not hold the written data\n");

    address_space_activate(NULL);
    address_space_destroy(space);
}

void mm_selftest(void)
{
    mm_page_map_unmap_test();

    mm_page_translate_test();

    mm_kmalloc_test();

    mm_address_space_test();

    kprintf("MM self-test completed successfully\n");
}