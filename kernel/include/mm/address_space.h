#ifndef _MM_ADDRESS_SPACE_H
#define _MM_ADDRESS_SPACE_H

#include <stddef.h>
#include <stdint.h>
#include <arch/mmu.h>

/**
 * @brief A user address space: private user half, shared kernel half.
 *
 * Every page mapped in the user half belongs to the address space and is
 * freed with it, so shared or kernel frames must never be mapped here.
 */
typedef struct address_space
{
    mmu_root_t *root;
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
 * @brief Map fresh zeroed pages covering [address, address + size) for user access.
 *
 * @param address Page-aligned start, inside [USER_BASE, USER_TOP).
 * @param size Bytes to map (rounded up to whole pages).
 * @param flags MMU_WRITE / MMU_EXEC; MMU_USER is always added.
 * @return 0 on success, -1 on failure (pages mapped so far stay until destroy).
 */
int address_space_map(address_space_t *space, uintptr_t address, size_t size, unsigned int flags);

/**
 * @brief Copy kernel data into mapped user pages, without activating the space.
 *
 * @return 0 on success, -1 if part of the range is not mapped.
 */
int address_space_write(address_space_t *space, uintptr_t address, const void *data, size_t size);

/**
 * @brief Copy data out of mapped user pages, without activating the space.
 *
 * @param address Page-aligned start, inside [USER_BASE, USER_TOP).
 * @param data Buffer to copy into.
 * @param size Bytes to copy (rounded up to whole pages).
 * @return 0 on success, -1 if part of the range is not mapped user memory.
 */
int address_space_read(address_space_t *space, uintptr_t address, void *data, size_t size);

/**
 * @brief Make an address space current, or NULL for the kernel alone.
 */
void address_space_activate(address_space_t *space);

#endif // _MM_ADDRESS_SPACE_H