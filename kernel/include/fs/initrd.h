#ifndef _FS_INITRD_H_
#define _FS_INITRD_H_

#include <stddef.h>
#include <boot_info.h>

/**
 * Finds the "initrd" boot module, checks every tar header (panics on a corrupt
 * archive), builds a tree of nodes from it (files point into the archive, which
 * stays reserved) and mounts it as the VFS root "/". Once, after mm_init and
 * before anything looks up a path.
 *
 * @param boot_info Pointer to the boot information structure containing the initrd module.
 */
void initrd_init(const boot_info_t *boot_info);

#endif // _FS_INITRD_H_