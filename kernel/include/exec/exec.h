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
 * @param argv Argument vector for the executable.
 * @param envp Environment vector for the executable.
 * @param space Pointer to the address space pointer to be created.
 * @param entry Pointer to store the entry point of the executable.
 * @param stack Pointer to store the initial stack pointer of the executable.
 * @return 0 on success, negative error code on failure.
 */
int exec_load(const void *image, size_t size, char *const argv[], char *const envp[], struct address_space **space, uintptr_t *entry, uintptr_t *stack);

#endif /* _EXEC_EXEC_H_ */