#ifndef _FS_DEVFS_H_
#define _FS_DEVFS_H_

/**
 * Creates the device file system and mounts it at /dev (panics if it cannot).
 * Once, after initrd_init (the mount point is in the initrd).
 */
void devfs_init(void);

#endif // _FS_DEVFS_H_