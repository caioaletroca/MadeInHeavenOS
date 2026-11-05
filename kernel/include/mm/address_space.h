#ifndef _MM_ADDRESS_SPACE_H
#define _MM_ADDRESS_SPACE_H

#include <stddef.h>
#include <stdint.h>
#include <arch/mmu.h>

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