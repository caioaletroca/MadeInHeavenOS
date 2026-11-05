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

static unsigned int page_refs(physaddr_t physical)
{
    return frame_lookup(physical)->refs;
}

/**
 * Tests that a clone shares its pages until one is written, counting each
 * frame's references, down to the frames being freed with the last sharer.
 */
static void mm_copy_on_write_test(void)
{
    static const char before[] = "before";
    static const char after[] = "after";
    const uintptr_t writable = USER_BASE;
    const uintptr_t read_only = USER_BASE + PAGE_SIZE;
    char text[sizeof(before)];

    address_space_t *space = address_space_create();
    KASSERT(space != NULL);
    KASSERT(address_space_map(space, writable, PAGE_SIZE, MMU_WRITE) == 0);
    KASSERT(address_space_map(space, read_only, PAGE_SIZE, 0) == 0);
    KASSERT(address_space_write(space, writable, before, sizeof(before)) == 0);

    // A read-only page refuses a write, but not the loader's fill
    KASSERT(address_space_write(space, read_only, before, sizeof(before)) != 0);
    KASSERT(address_space_fill(space, read_only, before, sizeof(before)) == 0);

    physaddr_t shared, code, copied, other;
    KASSERT(arch_mmu_translate(space->root, writable, &shared) == 0);
    KASSERT(arch_mmu_translate(space->root, read_only, &code) == 0);
    KASSERT(page_refs(shared) == 1 && page_refs(code) == 1);

    // Clones map the same frames
    address_space_t *copy = address_space_clone(space);
    KASSERT(copy != NULL);
    address_space_t *third = address_space_clone(copy);
    KASSERT(third != NULL);

    KASSERT(arch_mmu_translate(copy->root, writable, &copied) == 0 && copied == shared);
    KASSERT(arch_mmu_translate(third->root, read_only, &other) == 0 && other == code);
    KASSERT(page_refs(shared) == 3 && page_refs(code) == 3);

    // A write gets its own frame, with the old content, and the others do not see it
    KASSERT(address_space_write(copy, writable + sizeof(before) - 1, "!", 1) == 0);
    KASSERT(arch_mmu_translate(copy->root, writable, &copied) == 0 && copied != shared);
    KASSERT(page_refs(shared) == 2 && page_refs(copied) == 1);

    KASSERT(address_space_read(copy, writable, text, sizeof(before) - 1) == 0);
    KASSERT(memcmp(text, before, sizeof(before) - 1) == 0);
    KASSERT(address_space_read(space, writable + sizeof(before) - 1, text, 1) == 0 && text[0] == '\0');

    // A second write to the same page copies nothing
    KASSERT(address_space_write(copy, writable, after, sizeof(after)) == 0);
    KASSERT(arch_mmu_translate(copy->root, writable, &other) == 0 && other == copied);

    // The last sharer keeps the frame: no copy
    address_space_destroy(third);
    KASSERT(page_refs(shared) == 1 && page_refs(code) == 2);
    KASSERT(address_space_write(space, writable, after, sizeof(after)) == 0);
    KASSERT(arch_mmu_translate(space->root, writable, &other) == 0 && other == shared);

    // Sharing does not make a read-only page writable
    KASSERT(address_space_write(copy, read_only, after, sizeof(after)) != 0);
    KASSERT(page_refs(code) == 2);

    address_space_destroy(copy);
    KASSERT(page_refs(copied) == 0 && page_refs(code) == 1);

    address_space_destroy(space);
    KASSERT(page_refs(shared) == 0 && page_refs(code) == 0);
}

/**
 * Tests that the stack's pages appear on first use, up to the limit and no
 * further, and that loading a program does not create them.
 */
static void mm_stack_growth_test(void)
{
    const uintptr_t top_page = USER_STACK_TOP - PAGE_SIZE;
    const uintptr_t lowest = USER_STACK_TOP - USER_STACK_MAX;
    uint64_t word = 1;
    physaddr_t physical;

    address_space_t *space = address_space_create();
    KASSERT(space != NULL);
    KASSERT(arch_mmu_translate(space->root, top_page, &physical) != 0);

    KASSERT(address_space_fill(space, top_page, &word, sizeof(word)) != 0);
    KASSERT(arch_mmu_translate(space->root, top_page, &physical) != 0);

    KASSERT(address_space_write(space, top_page, &word, sizeof(word)) == 0);
    KASSERT(arch_mmu_translate(space->root, top_page, &physical) == 0);

    // A read grows it too, and a new page is zero
    KASSERT(address_space_read(space, lowest, &word, sizeof(word)) == 0 && word == 0);

    // Just outside, on both ends
    KASSERT(address_space_read(space, lowest - PAGE_SIZE, &word, sizeof(word)) != 0);
    KASSERT(address_space_write(space, lowest - PAGE_SIZE, &word, sizeof(word)) != 0);
    KASSERT(address_space_write(space, USER_STACK_TOP, &word, sizeof(word)) != 0);
    KASSERT(arch_mmu_translate(space->root, lowest - PAGE_SIZE, &physical) != 0);

    address_space_destroy(space);
}

void mm_selftest(void)
{
    mm_page_map_unmap_test();

    mm_page_translate_test();

    mm_kmalloc_test();

    mm_address_space_test();

    mm_copy_on_write_test();

    mm_stack_growth_test();
}