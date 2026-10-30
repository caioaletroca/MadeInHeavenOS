#ifndef _SELFTEST_VFS_H_
#define _SELFTEST_VFS_H_

#include <driver/block.h>

/**
 * Self-test for the VFS over the root's tree: path lookup (types, repeated
 * slashes, "." and "..", ENOENT/ENOTDIR/ENAMETOOLONG), one node per file,
 * reads at offsets and across the end, EISDIR for directories, listings,
 * mounts, devices, and reference counts back where they started.
 *
 * @param root_device The disk mounted as "/" (root_init), or NULL for the
 * initrd: the initrd's own layout, modes and EROFS are checked only then.
 */
void vfs_selftest(const block_device_t *root_device);

#endif // _SELFTEST_VFS_H_
