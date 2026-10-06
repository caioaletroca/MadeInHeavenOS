#ifndef _EXEC_EXEC_H_
#define _EXEC_EXEC_H_

#include <stdint.h>
#include <stddef.h>

struct address_space;

/**
 * Load an executable image into a new address space.
 *
 * @param image Pointer to the executable image in memory.
 * @param size Size of the executable image.
 * @param space Pointer to the address space pointer to be created.
 * @param entry Pointer to store the entry point of the executable.
 * @return 0 on success, negative error code on failure.
 */
int exec_load(const void *image, size_t size, struct address_space **space, uintptr_t *entry);

#endif /* _EXEC_EXEC_H_ */