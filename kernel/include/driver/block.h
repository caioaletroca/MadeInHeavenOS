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
} block_ops_t;

/**
 * A disk (or, later, a partition): a numbered array of 512-byte sectors.
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
 * @brief Find a block device by name.
 *
 * @param name The name of the block device to find.
 * @return A pointer to the block device if found, or NULL if not found.
 */
block_device_t *block_find(const char *name);

/**
 * @brief Read data from a block device.
 *
 * @param dev The block device to read from.
 * @param sector The starting sector to read from.
 * @param count The number of sectors to read.
 * @param buffer The buffer to store the read data.
 * @return 0 on success, or a negative error code on failure.
 */
int block_read(block_device_t *dev, uint64_t sector, uint32_t count, void *buffer);

/**
 * @brief Read bytes from a block device, at any offset and of any length.
 *
 * Reads whole sectors underneath; a partial first or last sector goes through a
 * one-sector buffer. The cache will take that buffer's place.
 *
 * @param dev The block device to read from.
 * @param buffer A kernel buffer of at least count bytes.
 * @param count The number of bytes to read.
 * @param offset The byte offset on the device.
 * @return The bytes read (fewer at the end of the device, 0 at or past it), or -errno.
 */
long block_read_bytes(block_device_t *dev, void *buffer, size_t count, uint64_t offset);

/**
 * @brief Publish every registered block device in /dev (mode 0660).
 * Once, after devfs_init: disks are found before the allocator and devfs exist.
 */
void block_devfs_init(void);

#endif /* _DRIVER_BLOCK_H_ */