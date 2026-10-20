#include <driver/block.h>
#include <fs/devfs.h>
#include <mihos/errno.h>
#include <stdint.h>
#include <string.h>
#include <panic.h>
#include <util.h>

static block_device_t *devices[BLOCK_DEVICES_MAX];

int block_register(block_device_t *dev)
{
    block_device_t *exists = block_find(dev->name);
    if (exists != NULL)
        return -EEXIST; // Device with the same name already exists

    for (int i = 0; i < BLOCK_DEVICES_MAX; i++)
    {
        if (devices[i] == NULL)
        {
            devices[i] = dev;
            return 0;
        }
    }

    return -ENOMEM; // No available slot
}

block_device_t *block_find(const char *name)
{
    for (int i = 0; i < BLOCK_DEVICES_MAX; i++)
    {
        if (devices[i] != NULL && strcmp(devices[i]->name, name) == 0)
        {
            return devices[i];
        }
    }
    return NULL;
}

int block_read(block_device_t *dev, uint64_t sector, uint32_t count, void *buffer)
{
    if (count == 0)
        return 0; // Nothing to read if count is 0

    if (sector >= dev->sector_count || count > dev->sector_count - sector)
        return -EINVAL; // Invalid sector or count

    return dev->ops->read(dev, sector, count, buffer);
}

long block_read_bytes(block_device_t *dev, void *buffer, size_t count, uint64_t offset)
{
    uint64_t size = dev->sector_count * BLOCK_SECTOR_SIZE;
    if (offset >= size)
        return 0;

    count = MIN(count, size - offset);

    uint8_t *buf = (uint8_t *)buffer;
    size_t done = 0;
    while (done < count)
    {
        uint64_t position = offset + done;
        uint64_t sector = position / BLOCK_SECTOR_SIZE;
        uint64_t within = position % BLOCK_SECTOR_SIZE;
        uint64_t remaining = count - done;

        // Aligned with at least one whole sector left: straight into the caller's buffer
        if (within == 0 && remaining >= BLOCK_SECTOR_SIZE)
        {
            uint32_t n = MIN(remaining / BLOCK_SECTOR_SIZE, UINT32_MAX);

            int ret = block_read(dev, sector, n, buf + done);
            if (ret < 0)
                return ret;
            done += (size_t)n * BLOCK_SECTOR_SIZE;
            continue;
        }

        // Handle unaligned reads by using a bounce buffer
        uint8_t bounce[BLOCK_SECTOR_SIZE];
        int ret = block_read(dev, sector, 1, bounce);
        if (ret < 0)
            return ret;

        uint64_t chunk = MIN(remaining, BLOCK_SECTOR_SIZE - within);
        memcpy(buf + done, bounce + within, chunk);
        done += chunk;
    }

    return (long)done;
}

void block_devfs_init(void)
{
    for (int i = 0; i < BLOCK_DEVICES_MAX; i++)
    {
        block_device_t *dev = devices[i];
        if (devices[i] != NULL)
        {
            int ret = devfs_register_block(dev->name, 0660, dev);
            if (ret < 0)
                panic("block_devfs_init: failed to register block device %s with error %d\n", dev->name, ret);
        }
    }
}