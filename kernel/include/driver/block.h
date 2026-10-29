#ifndef _DRIVER_BLOCK_H_
#define _DRIVER_BLOCK_H_

#include <stdint.h>
#include <stddef.h>

#define BLOCK_SECTOR_SIZE 512
#define BLOCK_NAME_MAX 16
#define BLOCK_DEVICES_MAX 8

struct block_device;

typedef struct block_ops
{
    int (*read)(struct block_device *dev, uint64_t sector, uint32_t count, void *buffer);
    int (*write)(struct block_device *dev, uint64_t sector, uint32_t count, const void *buffer);
    int (*flush)(struct block_device *dev);
} block_ops_t;

/**
 * A disk or a partition (driver/partition.c): a numbered array of 512-byte sectors.
 */
typedef struct block_device
{
    char name[BLOCK_NAME_MAX];
    uint64_t sector_count;
    uint64_t rdev; // MIHOS_MKDEV(major, minor), chosen by the driver
    const block_ops_t *ops;
    void *private; // the driver's own data (for ATA: which channel and drive)
} block_device_t;

/**
 * @brief Register a block device.
 *
 * @param dev The block device to register.
 * @return 0 on success, or a negative error code on failure.
 */
int block_register(block_device_t *dev);

/**
 * @brief Get the block device at the specified index
 *
 * @param index: The index of the block device
 * @return The block device at the specified index, or NULL if out of range
 */
block_device_t *block_at(size_t index);

/**
 * @brief Find a block device by name.
 *
 * @param name The name of the block device to find.
 * @return A pointer to the block device if found, or NULL if not found.
 */
block_device_t *block_find(const char *name);

/**
 * @brief Read sectors from a block device, straight from the driver.
 *
 * Only the block cache calls this (and the block self-test, to compare): a
 * read around the cache would miss what is cached, and once there are writes,
 * see stale data. Everything else reads through block_read_bytes or block_cache_get.
 *
 * @param dev The block device to read from.
 * @param sector The starting sector to read from.
 * @param count The number of sectors to read.
 * @param buffer The buffer to store the read data.
 * @return 0 on success, or a negative error code on failure.
 */
int block_read(block_device_t *dev, uint64_t sector, uint32_t count, void *buffer);

/**
 * @brief Write sectors to a block device, straight to the driver.
 *
 * Only the block cache calls this (and the block self-test): a write around the
 * cache would leave a cached copy of those sectors stale.
 *
 * @param dev The block device to write to.
 * @param sector The starting sector to write to.
 * @param count The number of sectors to write.
 * @param buffer The data, count * BLOCK_SECTOR_SIZE bytes.
 * @return 0 on success, -EINVAL past the end, -EROFS if the driver cannot write, or the driver's error.
 */
int block_write(block_device_t *dev, uint64_t sector, uint32_t count, const void *buffer);

/**
 * @brief Make every write the device acknowledged permanent (empty the disk's own write cache).
 *
 * @param dev The block device to flush.
 * @return 0 on success (also when the driver has no flush), or the driver's error.
 */
int block_flush(block_device_t *dev);

/**
 * @brief Read bytes from a block device, at any offset and of any length.
 *
 * Reads through the block cache: one block_cache_get per 4 KiB block the range
 * touches, the slice copied out, the buffer put back at once.
 *
 * @param dev The block device to read from.
 * @param buffer A kernel buffer of at least count bytes.
 * @param count The number of bytes to read.
 * @param offset The byte offset on the device.
 * @return The bytes read (fewer at the end of the device, 0 at or past it), or -errno.
 */
long block_read_bytes(block_device_t *dev, void *buffer, size_t count, uint64_t offset);

/**
 * @brief Write bytes to a block device, at any offset and of any length.
 *
 * Writes into the block cache: one block_cache_get per 4 KiB block the range
 * touches, the slice copied in between begin_write and end_write. The data
 * reaches the disk at the buffer's eviction or the next block_cache_sync.
 *
 * @param dev The block device to write to.
 * @param buffer A kernel buffer of at least count bytes.
 * @param count The number of bytes to write.
 * @param offset The byte offset on the device.
 * @return The bytes written (fewer at the end of the device), -ENOSPC at or past it, or -errno.
 */
long block_write_bytes(block_device_t *dev, const void *buffer, size_t count, uint64_t offset);

/**
 * @brief Publish every registered block device in /dev (mode 0660).
 * Once, after devfs_init: disks are found before the allocator and devfs exist.
 */
void block_devfs_init(void);

#endif /* _DRIVER_BLOCK_H_ */