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

#endif /* _DRIVER_BLOCK_H_ */