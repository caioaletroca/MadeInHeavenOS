#ifndef _DRIVER_PARTITION_H_
#define _DRIVER_PARTITION_H_

/**
 * @brief Read every disk's MBR and register its primary partitions as block devices
 * (hdb1 … hdb4). Once, after irq_enable (it reads the disks) and before
 * block_devfs_init (so they show in /dev).
 */
void partition_init(void);

#endif // _DRIVER_PARTITION_H_