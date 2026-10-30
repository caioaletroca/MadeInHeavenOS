#ifndef _FS_DEVFS_H_
#define _FS_DEVFS_H_

#include <stdint.h>

struct file_ops;

/**
 * @brief Creates the device file system's root, empty. Devices can register from
 * here on; it is reachable as /dev only after devfs_mount. Once, after mm_init.
 */
void devfs_init(void);

/**
 * @brief Mounts the device file system at /dev (panics if it cannot).
 * Once, after the root file system is mounted (the mount point is in it).
 */
void devfs_mount(void);

/**
 * @brief Registers a new device node in the /dev directory.
 *
 * @param name The name of the device node.
 * @param mode The file mode (permissions) of the device node.
 * @param rdev The device number (major and minor) for the device node.
 * @param ops The file operations associated with the device node.
 * @return Returns 0 on success, or a negative error code on failure.
 */
int devfs_register(const char *name, uint32_t mode, uint64_t rdev, const struct file_ops *ops);

struct block_device;

/**
 * @brief Registers a block device node in /dev, read through the block layer.
 *
 * @param name The device node's name.
 * @param mode The permission bits.
 * @param dev The block device; its rdev and size go into the node.
 * @return 0 on success, -EINVAL, -EEXIST or -ENOMEM.
 */
int devfs_register_block(const char *name, uint32_t mode, struct block_device *dev);

#endif // _FS_DEVFS_H_