#ifndef _EXEC_EXEC_H_
#define _EXEC_EXEC_H_

#include <stdint.h>
#include <stddef.h>
#include <boot_info.h>

struct address_space;

/**
 * Initialize the exec module system with the given boot information.
 *
 * @param info The boot information containing module data.
 */
void exec_modules_init(const boot_info_t *info);

/**
 * Find a boot module by its path.
 *
 * @param path The path of the module to find.
 * @return A pointer to the boot module if found, or NULL if not found.
 */
const boot_module_t *exec_module_find(const char *path);

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