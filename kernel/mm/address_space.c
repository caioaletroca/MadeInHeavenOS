#include <mm/address_space.h>
#include <mm/kmalloc.h>
#include <mm/frame.h>
#include <asm/memory.h>
#include <string.h>
#include <addresses.h>
#include <util.h>

address_space_t *address_space_create(void)
{
    address_space_t *space = kzalloc(sizeof(address_space_t));

    if (space == NULL)
        return NULL;

    space->root = arch_mmu_root_create();

    if (space->root == NULL)
    {
        kfree(space);
        return NULL;
    }

    return space;
}

void address_space_destroy(address_space_t *space)
{
    if (space == NULL)
        return;

    arch_mmu_root_destroy(space->root);
    kfree(space);
}

/**
 * Check that [address, address + size) lies inside the user half.
 */
static bool address_space_range_valid(uintptr_t address, size_t size)
{
    return address >= USER_BASE && address <= USER_TOP && size <= USER_TOP - address;
}

int address_space_map(address_space_t *space, uintptr_t address, size_t size, unsigned int flags)
{
    if ((address & (PAGE_SIZE - 1)) != 0)
        return -1;

    size = ALIGN_UP(size, PAGE_SIZE);

    if (!address_space_range_valid(address, size))
        return -1;

    for (uintptr_t page = address; page < address + size; page += PAGE_SIZE)
    {
        physaddr_t physical = frame_alloc(0, 0);
        if (physical == 0)
            return -1;

        // Never hand a process another process's (or the kernel's) old data
        memset(phys_to_kern(physical), 0, PAGE_SIZE);

        if (arch_mmu_map(space->root, page, physical, flags | MMU_USER) != 0)
        {
            frame_free(physical, 0);
            return -1;
        }
    }

    return 0;
}

int address_space_write(address_space_t *space, uintptr_t address, const void *data, size_t size)
{
    const uint8_t *source = data;

    if (!address_space_range_valid(address, size))
        return -1;

    while (size > 0)
    {
        uintptr_t page = ALIGN_DOWN(address, PAGE_SIZE);
        size_t offset = address - page;
        size_t chunk = PAGE_SIZE - offset;
        physaddr_t physical;

        if (chunk > size)
            chunk = size;

        if (arch_mmu_translate(space->root, page, &physical) != 0)
            return -1;

        // Write through the kernel's direct map, so the space need not be active
        memcpy((uint8_t *)phys_to_kern(physical) + offset, source, chunk);

        address += chunk;
        source += chunk;
        size -= chunk;
    }

    return 0;
}

void address_space_activate(address_space_t *space)
{
    arch_mmu_activate(space != NULL ? space->root : NULL);
}