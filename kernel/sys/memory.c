#include "internal.h"
#include <mihos/mman.h>

// ---- The user heap: brk ----

/**
 * brk(address): changes the end of the data segment (heap) to the specified address.
 */
long sys_brk(uintptr_t address)
{
    address_space_t *space = syscall_space();
    if (space == NULL)
        return -ENOMEM;

    uintptr_t limit = USER_MAP_TOP;

    if (address < space->heap_start || address > limit)
        return (long)space->brk;

    uintptr_t old_end = ALIGN_UP(space->brk, PAGE_SIZE);
    uintptr_t new_end = ALIGN_UP(address, PAGE_SIZE);

    if (new_end > old_end &&
        (!address_space_range_free(space, old_end, new_end) ||
         address_space_map(space, old_end, new_end - old_end, MMU_WRITE) != 0))
        return (long)space->brk;

    if (new_end < old_end)
        address_space_unmap(space, new_end, old_end - new_end);

    space->brk = address;
    return (long)space->brk;
}

// ---- Mappings: mmap, munmap and mprotect ----

/**
 * mmap(address, length, protection, flags, fd, offset): reserves anonymous private
 * memory; its pages are mapped zeroed as they are touched. Returns the start.
 */
long sys_mmap(uintptr_t address, size_t length, unsigned int protection, unsigned int flags, long fd, long offset)
{
    (void)fd; // anonymous memory only: no file, no offset
    (void)offset;

    address_space_t *space = syscall_space();
    if (space == NULL)
        return -ENOMEM;

    // Exactly one of MAP_SHARED and MAP_PRIVATE; no flag or protection bit we do not know
    unsigned int sharing = flags & (MAP_SHARED | MAP_PRIVATE);
    if (sharing == 0 || sharing == (MAP_SHARED | MAP_PRIVATE) ||
        (flags & ~(MAP_SHARED | MAP_PRIVATE | MAP_FIXED | MAP_ANONYMOUS)) != 0 ||
        (protection & ~(PROT_READ | PROT_WRITE | PROT_EXEC)) != 0)
        return -EINVAL;

    // Files, and memory shared across fork, come later
    if ((flags & MAP_ANONYMOUS) == 0 || sharing == MAP_SHARED)
        return -ENODEV;

    int error = address_space_reserve(space, &address, length, protection, (flags & MAP_FIXED) != 0);
    return error != 0 ? error : (long)address;
}

/**
 * munmap(address, length): removes the mappings in the range; the rest stays.
 */
long sys_munmap(uintptr_t address, size_t length)
{
    address_space_t *space = syscall_space();
    if (space == NULL)
        return -EINVAL;

    return address_space_release(space, address, length);
}

/**
 * mprotect(address, length, protection): changes the protection of mapped memory;
 * the range must be inside mappings, with no gap.
 */
long sys_mprotect(uintptr_t address, size_t length, unsigned int protection)
{
    if ((protection & ~(PROT_READ | PROT_WRITE | PROT_EXEC)) != 0)
        return -EINVAL;

    address_space_t *space = syscall_space();
    if (space == NULL)
        return -ENOMEM;

    return address_space_protect(space, address, length, protection);
}