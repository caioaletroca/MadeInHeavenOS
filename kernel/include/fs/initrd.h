#ifndef _FS_INITRD_H_
#define _FS_INITRD_H_

#include <stddef.h>
#include <boot_info.h>

/**
 * Initializes the initial ramdisk (initrd) subsystem.
 * This function must be called early during the boot process.
 * It locates the initrd boot module and prepares it for file access.
 *
 * @param boot_info Pointer to the boot information structure containing the initrd module.
 */
void initrd_init(const boot_info_t *boot_info);

/**
 * Finds a file within the initial ramdisk (initrd) by its path.
 *
 * @param path The path to the file within the initrd. Must start with '/'.
 * @param size Pointer to a variable where the size of the found file will be stored. Can be NULL.
 * @return Pointer to the file data if found, or NULL if the file does not exist.
 */
const void *initrd_find(const char *path, size_t *size);

#endif // _FS_INITRD_H_