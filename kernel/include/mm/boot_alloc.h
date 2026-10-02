#ifndef _BOOT_ALLOC_H_
#define _BOOT_ALLOC_H_

#include <stddef.h>
#include <addresses.h>

/**
 * Initialize the boot allocator with the given memory range.
 *
 * @param start		Physical start address of the memory range
 * @param size		Size of the memory range in bytes
 */
void boot_alloc_init(physaddr_t start, size_t size);

/**
 * Allocate memory from the boot allocator with the specified size and alignment.
 *
 * @param size		Size of the memory to allocate in bytes
 * @param alignment	Alignment requirement for the allocated memory
 * @return			Pointer to the allocated memory, or NULL if allocation fails
 */
void *boot_alloc(size_t size, size_t alignment);

/**
 * @brief Forbid further boot allocations (kmalloc is available from now on).
 */
void boot_alloc_seal(void);

#endif /* _BOOT_ALLOC_H_ */