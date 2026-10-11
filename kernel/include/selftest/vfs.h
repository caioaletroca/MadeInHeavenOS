#ifndef _SELFTEST_VFS_H_
#define _SELFTEST_VFS_H_

/**
 * Self-test for the VFS over the initrd tree: path lookup (types, repeated
 * slashes, "." and "..", ENOENT/ENOTDIR/ENAMETOOLONG), one node per file,
 * reads at offsets and across the end, EISDIR for directories, and reference
 * counts back where they started.
 */
void vfs_selftest(void);

#endif // _SELFTEST_VFS_H_
