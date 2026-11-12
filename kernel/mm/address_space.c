#include <mm/address_space.h>
#include <mm/kmalloc.h>
#include <mm/frame.h>
#include <asm/memory.h>
#include <mihos/mman.h>
#include <mihos/errno.h>
#include <string.h>
#include <addresses.h>
#include <util.h>

/**
 * Check that [address, address + size) lies inside the user half.
 */
static bool address_space_range_valid(uintptr_t address, size_t size)
{
    return address >= USER_BASE && address <= USER_TOP && size <= USER_TOP - address;
}

static unsigned int region_mmu_flags(unsigned int protection)
{
    return (protection & PROT_WRITE ? MMU_WRITE : 0) | (protection & PROT_EXEC ? MMU_EXEC : 0);
}

static region_t *region_find(address_space_t *space, uintptr_t address)
{
    list_t *node;
    list_for_each(node, &space->regions)
    {
        region_t *region = list_container(node, region_t, link);
        if (address < region->start)
            break;
        if (address < region->end)
            return region;
    }

    return NULL;
}

static void region_insert(address_space_t *space, region_t *region)
{
    list_t *node;
    list_for_each(node, &space->regions)
    {
        if (list_container(node, region_t, link)->start > region->start)
            break;
    }

    list_insert_before(node, &region->link);
}

static uintptr_t region_place(address_space_t *space, size_t size)
{
    uintptr_t bottom = ALIGN_UP(space->brk, PAGE_SIZE);
    uintptr_t top = USER_MAP_TOP;

    for (list_t *node = space->regions.prev; node != &space->regions; node = node->prev)
    {
        region_t *region = list_container(node, region_t, link);
        if (top - region->end >= size)
            return top - size;
        top = region->start;
    }

    return top >= bottom && top - bottom >= size ? top - size : 0;
}

bool address_space_range_free(address_space_t *space, uintptr_t start, uintptr_t end)
{
    list_t *node;
    list_for_each(node, &space->regions)
    {
        region_t *region = list_container(node, region_t, link);
        if (region->start >= end)
            break;
        if (region->end > start)
            return false;
    }

    return true;
}

/**
 * Copy data between the kernel and a user address space.
 * to_user: true if copying to the user space, false if copying from the user space.
 * as_user: true if the access should be treated as a user access (subject to faults), false otherwise.
 * Returns 0 on success, -1 on failure.
 */
static int address_space_copy(address_space_t *space, uintptr_t address, void *data, size_t size, bool to_user, bool as_user)
{
    uint8_t *kernel = data;

    if (space == NULL || !address_space_range_valid(address, size))
        return -1;

    while (size > 0)
    {
        uintptr_t page = ALIGN_DOWN(address, PAGE_SIZE);
        size_t offset = address - page;
        size_t chunk = PAGE_SIZE - offset;
        physaddr_t physical;

        if (chunk > size)
            chunk = size;

        // The kernel reaches the frame directly, so no fault will do this for it: what a
        // program's access would resolve has to happen here. Before the translate: this may
        // change the frame
        if (as_user && address_space_fault(space, page, to_user) != 0)
            return -1;

        if (arch_mmu_translate(space->root, page, &physical) != 0)
            return -1;

        uint8_t *user = (uint8_t *)phys_to_kern(physical) + offset;

        if (to_user)
            memcpy(user, kernel, chunk);
        else
            memcpy(kernel, user, chunk);

        address += chunk;
        kernel += chunk;
        size -= chunk;
    }

    return 0;
}

address_space_t *address_space_create(void)
{
    address_space_t *space = kzalloc(sizeof(address_space_t));
    if (space == NULL)
        return NULL;

    list_init(&space->regions);
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
    list_t *node, *next;
    list_for_each_safe(node, next, &space->regions)
        kfree(list_container(node, region_t, link));
    kfree(space);
}

address_space_t *address_space_clone(address_space_t *space)
{
    address_space_t *copy = address_space_create();
    if (copy == NULL)
        return NULL;

    if (arch_mmu_root_copy(copy->root, space->root) != 0)
    {
        address_space_destroy(copy);
        return NULL;
    }

    copy->heap_start = space->heap_start;
    copy->brk = space->brk;

    list_t *node;
    list_for_each(node, &space->regions)
    {
        region_t *region = kmalloc(sizeof(region_t));
        if (region == NULL)
        {
            address_space_destroy(copy);
            return NULL;
        }
        *region = *list_container(node, region_t, link);
        list_insert_before(&copy->regions, &region->link);
    }

    return copy;
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
        {
            address_space_unmap(space, address, page - address);
            return -1;
        }

        // Never hand a process another process's (or the kernel's) old data
        memset(phys_to_kern(physical), 0, PAGE_SIZE);

        if (arch_mmu_map(space->root, page, physical, flags | MMU_USER) != 0)
        {
            address_space_unmap(space, address, page - address);
            frame_free(physical, 0);
            return -1;
        }
    }

    return 0;
}

int address_space_unmap(address_space_t *space, uintptr_t address, size_t size)
{
    if ((address & (PAGE_SIZE - 1)) != 0)
        return -1;

    if (space == NULL || !address_space_range_valid(address, size))
        return -1;

    // Round up the size to whole pages before unmapping
    size = ALIGN_UP(size, PAGE_SIZE);

    // Unmap each page in the specified range
    for (uintptr_t page = address; page < address + size; page += PAGE_SIZE)
    {
        physaddr_t physical;
        if (arch_mmu_translate(space->root, page, &physical) != 0)
            continue;

        arch_mmu_unmap(space->root, page);
        frame_free(physical, 0);
    }

    return 0;
}

int address_space_reserve(address_space_t *space, uintptr_t *address, size_t size, unsigned int protection, bool fixed)
{
    uintptr_t bottom = ALIGN_UP(space->brk, PAGE_SIZE);
    uintptr_t start = *address;

    if (size == 0)
        return -EINVAL;
    if (size > USER_MAP_TOP)
        return -ENOMEM;
    size = ALIGN_UP(size, PAGE_SIZE);

    bool aligned = (start & (PAGE_SIZE - 1)) == 0;
    bool inside = start >= bottom && start <= USER_MAP_TOP && size <= USER_MAP_TOP - start;

    if (fixed)
    {
        if (!aligned)
            return -EINVAL;
        if (!inside)
            return -ENOMEM;
    }
    else if (!aligned || !inside || !address_space_range_free(space, start, start + size))
    {
        start = region_place(space, size);
        if (start == 0)
            return -ENOMEM;
    }

    region_t *region = kmalloc(sizeof(region_t));
    if (region == NULL)
        return -ENOMEM;

    // MAP_FIXED replaces whatever was there
    if (fixed && address_space_release(space, start, size) != 0)
    {
        kfree(region);
        return -ENOMEM;
    }

    region->start = start;
    region->end = start + size;
    region->protection = protection;
    region_insert(space, region);

    *address = start;
    return 0;
}

int address_space_release(address_space_t *space, uintptr_t address, size_t size)
{
    if ((address & (PAGE_SIZE - 1)) != 0 || size == 0 || !address_space_range_valid(address, size))
        return -EINVAL;

    uintptr_t end = address + ALIGN_UP(size, PAGE_SIZE);

    // Cutting the middle out of a region leaves two: the second is allocated first, so a
    // failure changes nothing
    region_t *spare = kmalloc(sizeof(region_t));
    if (spare == NULL)
        return -ENOMEM;

    list_t *node, *next;
    list_for_each_safe(node, next, &space->regions)
    {
        region_t *region = list_container(node, region_t, link);
        if (region->start >= end)
            break;
        if (region->end <= address)
            continue;

        uintptr_t cut_start = MAX(region->start, address);
        uintptr_t cut_end = MIN(region->end, end);
        address_space_unmap(space, cut_start, cut_end - cut_start);

        if (region->start < address && region->end > end)
        {
            *spare = *region;
            spare->start = end;
            region->end = address;
            list_insert_after(&region->link, &spare->link);
            spare = NULL;
            break;
        }

        if (region->start < address) // its tail
            region->end = address;
        else if (region->end > end) // its head
            region->start = end;
        else // all of it
        {
            list_remove(&region->link);
            kfree(region);
        }
    }

    kfree(spare);
    return 0;
}

int address_space_fault(address_space_t *space, uintptr_t address, bool write)
{
    uintptr_t page = ALIGN_DOWN(address, PAGE_SIZE);
    physaddr_t physical;

    if (arch_mmu_translate(space->root, page, &physical) == 0)
        return write ? arch_mmu_prepare_write(space->root, page) : 0;

    // The stack grows into its reservation
    if (page >= USER_STACK_TOP - USER_STACK_MAX && page < USER_STACK_TOP)
        return address_space_map(space, page, PAGE_SIZE, MMU_WRITE);

    // A region's page touched for the first time, if its protection allows the access
    region_t *region = region_find(space, page);
    if (region == NULL || region->protection == PROT_NONE ||
        (write && (region->protection & PROT_WRITE) == 0))
        return -1;

    return address_space_map(space, page, PAGE_SIZE, region_mmu_flags(region->protection));
}

int address_space_write(address_space_t *space, uintptr_t address, const void *data, size_t size)
{
    return address_space_copy(space, address, (void *)data, size, true, true);
}

int address_space_fill(address_space_t *space, uintptr_t address, const void *data, size_t size)
{
    return address_space_copy(space, address, (void *)data, size, true, false);
}

int address_space_read(address_space_t *space, uintptr_t address, void *data, size_t size)
{
    return address_space_copy(space, address, data, size, false, true);
}

void address_space_activate(address_space_t *space)
{
    arch_mmu_activate(space != NULL ? space->root : NULL);
}