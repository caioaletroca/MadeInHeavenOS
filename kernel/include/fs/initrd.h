#ifndef _FS_INITRD_H_
#define _FS_INITRD_H_

#include <fs/vfs.h>
#include <stddef.h>
#include <boot_info.h>

/**
 * Finds the "initrd" boot module, checks every tar header (panics on a corrupt
 * archive) and builds a tree of nodes from it (files point into the archive,
 * which stays reserved). Mounting it as "/" is the caller's. Once, after mm_init.
 *
 * @param boot_info Pointer to the boot information structure containing the initrd module.
 * @return The tree's root directory (one reference), or NULL without an "initrd" module.
 */
vfs_node_t *initrd_init(const boot_info_t *boot_info);

#endif // _FS_INITRD_H_