#ifndef _FS_ROOT_H_
#define _FS_ROOT_H_

#include <boot_info.h>
#include <kernel_args.h>
#include <driver/block.h>

/**
 * @brief Mounts "/": ext2 on the block device root= names, or the initrd without root=.
 *
 * Panics when root= is given but cannot be mounted (printing the block devices
 * that exist), and when there is neither root= nor an initrd. Once, after
 * block_devfs_init (the disk needs irq_enable) and before anything looks up a path.
 *
 * @param args The kernel's options (args->root).
 * @param boot_info Where the initrd module is.
 * @return The root's block device, or NULL when the initrd is "/".
 */
block_device_t *root_init(const kernel_args_t *args, const boot_info_t *boot_info);

#endif /* _FS_ROOT_H_ */