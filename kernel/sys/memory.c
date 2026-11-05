#include "internal.h"

// ---- The user heap: brk ----

/**
 * brk(address): changes the end of the data segment (heap) to the specified address.
 */
long sys_brk(uintptr_t address)
{
    address_space_t *space = syscall_space();
    if (space == NULL)
        return -ENOMEM;

    uintptr_t limit = USER_STACK_TOP - USER_STACK_MAX - PAGE_SIZE;

    if (address < space->heap_start || address > limit)
        return (long)space->brk;

    uintptr_t old_end = ALIGN_UP(space->brk, PAGE_SIZE);
    uintptr_t new_end = ALIGN_UP(address, PAGE_SIZE);

    if (new_end > old_end && address_space_map(space, old_end, new_end - old_end, MMU_WRITE) != 0)
        return (long)space->brk;

    if (new_end < old_end)
        address_space_unmap(space, new_end, old_end - new_end);

    space->brk = address;
    return (long)space->brk;
}
