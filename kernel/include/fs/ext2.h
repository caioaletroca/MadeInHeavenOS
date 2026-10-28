#ifndef _FS_EXT2_H_
#define _FS_EXT2_H_

#include <driver/block.h>
#include <fs/vfs.h>

/**
 * @brief Reads and checks an ext2 file system on a disk: writable, unless it has a
 * ro_compat feature we do not know or the disk cannot write.
 *
 * @param disk The block device.
 * @param root Receives the root directory's node (one reference).
 * @return 0, -EINVAL if it is not ext2 or needs a feature we do not support,
 *         -EIO if it is inconsistent, -ENOMEM, or the disk's error.
 */
int ext2_mount(block_device_t *disk, vfs_node_t **root);

/**
 * @brief Mounts the first disk holding ext2 on /mnt; logs either way, never panics.
 * Once, after block_devfs_init and block_cache_init.
 */
void ext2_init(void);

#endif //_FS_EXT2_H_