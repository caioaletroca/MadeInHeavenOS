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

/**
 * Turn a region's PROT_* protection into the MMU flags of its pages (MMU_USER
 * not included). PROT_READ needs no flag: a present page is always readable.
 */
static unsigned int region_mmu_flags(unsigned int protection)
{
    return (protection & PROT_WRITE ? MMU_WRITE : 0) | (protection & PROT_EXEC ? MMU_EXEC : 0);
}

/**
 * Find the region holding address, or NULL if none does.
 */
static region_t *region_find(address_space_t *space, uintptr_t address)
{
    list_t *node;
    list_for_each(node, &space->regions)
    {
        region_t *region = list_container(node, region_t, link);
        if (address < region->start)
            break; // sorted: no later region holds it
        if (address < region->end)
            return region;
    }

    return NULL;
}

/**
 * Insert a region into its place in the sorted list. Its range must be free.
 */
static void region_insert(address_space_t *space, region_t *region)
{
    list_t *node;
    list_for_each(node, &space->regions)
    {
        if (list_container(node, region_t, link)->start > region->start)
            break;
    }

    // Before the first region starting later; node is the head when none does, so it goes last
    list_insert_before(node, &region->link);
}

/**
 * Cut a region in two at `at`, an address inside it: region keeps [start, at),
 * spare becomes [at, end) with the same protection, right after it in the list.
 */
static void region_split(region_t *region, uintptr_t at, region_t *spare)
{
    *spare = *region;
    spare->start = at;
    region->end = at;
    list_insert_after(&region->link, &spare->link);
}

/**
 * The flags a region's mapped page gets when its protection changes. PROT_NONE
 * keeps the frame but drops MMU_USER, so ring 3 cannot reach it.
 */
static unsigned int region_page_flags(unsigned int protection)
{
    return protection == PROT_NONE ? 0 : region_mmu_flags(protection) | MMU_USER;
}

/**
 * Find room for a new region of size bytes (page aligned), top-down as Linux:
 * the highest gap below USER_MAP_TOP and above the heap's last page.
 *
 * @return The start of the gap's top size bytes, or 0 if no gap is big enough.
 */
static uintptr_t region_place(address_space_t *space, size_t size)
{
    uintptr_t bottom = ALIGN_UP(space->brk, PAGE_SIZE);
    uintptr_t top = USER_MAP_TOP;

    // From the highest region down: the gap between each one's end and top
    for (list_t *node = space->regions.prev; node != &space->regions; node = node->prev)
    {
        region_t *region = list_container(node, region_t, link);
        if (top - region->end >= size)
            return top - size;
        top = region->start;
    }

    // Last, the gap between the heap and the lowest region (or USER_MAP_TOP)
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
        start = region_place(space, size); // the address was only a hint
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

        // Free the pages of this region inside the range (only those ever touched are mapped)
        uintptr_t cut_start = MAX(region->start, address);
        uintptr_t cut_end = MIN(region->end, end);
        address_space_unmap(space, cut_start, cut_end - cut_start);

        if (region->start < address && region->end > end) // the middle: both ends stay
        {
            region_split(region, end, spare);
            region->end = address;
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

    kfree(spare); // NULL when used; kfree(NULL) does nothing
    return 0;
}

int address_space_protect(address_space_t *space, uintptr_t address, size_t size, unsigned int protection)
{
    if ((address & (PAGE_SIZE - 1)) != 0 || !address_space_range_valid(address, size))
        return -EINVAL;
    if (size == 0)
        return 0; // POSIX: nothing to change

    uintptr_t end = address + ALIGN_UP(size, PAGE_SIZE);

    // Every page must be in a region, checked before anything changes: covered moves
    // forward over regions that touch each other until it reaches end, or a gap stops it
    uintptr_t covered = address;
    list_t *node;
    list_for_each(node, &space->regions)
    {
        region_t *region = list_container(node, region_t, link);
        if (region->end <= covered)
            continue;
        if (region->start > covered)
            break; // a gap
        covered = region->end;
        if (covered >= end)
            break;
    }
    if (covered < end)
        return -ENOMEM;

    // At most two splits, one at each end of the range: both allocated first, so a
    // failure changes nothing
    region_t *spare_start = kmalloc(sizeof(region_t));
    region_t *spare_end = kmalloc(sizeof(region_t));
    if (spare_start == NULL || spare_end == NULL)
    {
        kfree(spare_start);
        kfree(spare_end);
        return -ENOMEM;
    }

    // Never NULL: the check above found both ends inside regions
    region_t *first = region_find(space, address);
    if (first->start < address)
    {
        region_split(first, address, spare_start);
        spare_start = NULL;
    }

    region_t *last = region_find(space, end - 1);
    if (last->end > end)
    {
        region_split(last, end, spare_end);
        spare_end = NULL;
    }

    // Now the range is whole regions: change them, and their pages already mapped
    list_for_each(node, &space->regions)
    {
        region_t *region = list_container(node, region_t, link);
        if (region->start >= end)
            break;
        if (region->end <= address)
            continue;

        region->protection = protection;
        for (uintptr_t page = region->start; page < region->end; page += PAGE_SIZE)
            arch_mmu_protect(space->root, page, region_page_flags(protection)); // -1: not touched yet, fine
    }

    kfree(spare_start);
    kfree(spare_end);
    return 0;
}

int address_space_fault(address_space_t *space, uintptr_t address, bool write)
{
    uintptr_t page = ALIGN_DOWN(address, PAGE_SIZE);
    physaddr_t physical;

    if (arch_mmu_translate(space->root, page, &physical) == 0)
    {
        // A page made PROT_NONE keeps its frame, but no access reaches it: the kernel's
        // copies come through here too, and the direct map would ignore the missing user bit
        region_t *region = region_find(space, page);
        if (region != NULL && region->protection == PROT_NONE)
            return -1;

        return write ? arch_mmu_prepare_write(space->root, page) : 0;
    }

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