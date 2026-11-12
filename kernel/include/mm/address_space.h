#ifndef _MM_ADDRESS_SPACE_H
#define _MM_ADDRESS_SPACE_H

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include <arch/mmu.h>
#include <sys/list.h>

/**
 * @brief A range of user memory reserved by mmap: its pages are mapped as they are touched.
 */
typedef struct region
{
    list_t link;             // in address_space_t.regions
    uintptr_t start;         // page aligned
    uintptr_t end;           // page aligned, exclusive
    unsigned int protection; // PROT_READ | PROT_WRITE | PROT_EXEC, or PROT_NONE
} region_t;

/**
 * @brief A user address space: private user half, shared kernel half.
 *
 * Every page mapped in the user half belongs to the address spaces mapping
 * it: frames are counted, and freed with the last one.
 */
typedef struct address_space
{
    mmu_root_t *root;
    uintptr_t heap_start;
    uintptr_t brk;
    list_t regions;
} address_space_t;

/**
 * @brief Create an empty address space.
 *
 * @return The address space, or NULL if out of memory.
 */
address_space_t *address_space_create(void);

/**
 * @brief Free an address space and every page mapped in it. It must not be active.
 */
void address_space_destroy(address_space_t *space);

/**
 * @brief Make a copy-on-write copy of an address space: pages are shared until written, and the heap’s limits are copied.
 *
 * @return The copy, or NULL if out of memory.
 */
address_space_t *address_space_clone(address_space_t *space);

/**
 * @brief Map fresh zeroed pages covering [address, address + size) for user access.
 *
 * @param address Page-aligned start, inside [USER_BASE, USER_TOP).
 * @param size Bytes to map (rounded up to whole pages).
 * @param flags MMU_WRITE / MMU_EXEC; MMU_USER is always added.
 * @return 0 on success, -1 on failure.
 */
int address_space_map(address_space_t *space, uintptr_t address, size_t size, unsigned int flags);

/**
 * @brief Unmap pages covering [address, address + size) from a user address space.
 *
 * @param space The address space from which to unmap pages.
 * @param address Page-aligned start, inside [USER_BASE, USER_TOP).
 * @param size Bytes to unmap (rounded up to whole pages).
 * @return 0 on success, -1 on failure.
 */
int address_space_unmap(address_space_t *space, uintptr_t address, size_t size);

/**
 * @brief Reserve [*address, *address + size) for user memory mapped as touched.
 *
 * @param address In: the wanted start (a hint unless fixed). Out: the start chosen.
 * @param fixed Exactly there, replacing any region in the way.
 * @return 0, -EINVAL (fixed and not page aligned) or -ENOMEM (no room, out of memory).
 */
int address_space_reserve(address_space_t *space, uintptr_t *address, size_t size, unsigned int protection, bool fixed);

/**
 * @brief Remove the regions inside [address, address + size), and free their pages.
 * Parts of regions outside the range stay. Memory outside regions is never touched.
 *
 * @return 0, -EINVAL (unaligned, empty or outside the user half) or -ENOMEM.
 */
int address_space_release(address_space_t *space, uintptr_t address, size_t size);

/**
 * @brief Whether no region overlaps [start, end).
 */
bool address_space_range_free(address_space_t *space, uintptr_t start, uintptr_t end);

/**
 * @brief Make a user page ready for an access, as a page fault would: a stack
 * page not touched yet is mapped, a shared one is copied before a write.
 *
 * @param space The address space.
 * @param address An address inside the page.
 * @param write Whether the access is a write.
 * @return 0 if the access can go ahead, -1 if it cannot.
 */
int address_space_fault(address_space_t *space, uintptr_t address, bool write);

/**
 * @brief Copy kernel data into user pages as a program's write would: read-only
 * pages refuse it, shared ones are copied first. Does not activate the space.
 *
 * @param space The address space to write into.
 * @param address Start, inside [USER_BASE, USER_TOP).
 * @param data Kernel buffer to copy from.
 * @param size Bytes to write.
 * @return 0 on success, -1 if part of the range cannot be written.
 */
int address_space_write(address_space_t *space, uintptr_t address, const void *data, size_t size);

/**
 * @brief Copy kernel data into mapped user pages, read-only ones too: for
 * loading a program into a space nothing runs in yet.
 *
 * @param space The address space to fill.
 * @param address Start, inside [USER_BASE, USER_TOP).
 * @param data Kernel buffer to copy from.
 * @param size Bytes to write.
 * @return 0 on success, -1 if part of the range is not mapped.
 */
int address_space_fill(address_space_t *space, uintptr_t address, const void *data, size_t size);

/**
 * @brief Copy data out of mapped user pages, without activating the space.
 *
 * @param address Start, inside [USER_BASE, USER_TOP).
 * @param data Buffer to copy into.
 * @param size Bytes to copy.
 * @return 0 on success, -1 if part of the range is not mapped user memory.
 */
int address_space_read(address_space_t *space, uintptr_t address, void *data, size_t size);

/**
 * @brief Make an address space current, or NULL for the kernel alone.
 */
void address_space_activate(address_space_t *space);

#endif // _MM_ADDRESS_SPACE_H