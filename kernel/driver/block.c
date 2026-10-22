#include <driver/block.h>
#include <driver/block_cache.h>
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

block_device_t *block_at(size_t index)
{
    return index < BLOCK_DEVICES_MAX ? devices[index] : NULL;
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

int block_write(block_device_t *dev, uint64_t sector, uint32_t count, const void *buffer)
{
    if (count == 0)
        return 0; // Nothing to write if count is 0

    if (sector >= dev->sector_count || count > dev->sector_count - sector)
        return -EINVAL; // Invalid sector or count

    if (dev->ops->write == NULL)
        return -EROFS; // Driver cannot write

    return dev->ops->write(dev, sector, count, buffer);
}

int block_flush(block_device_t *dev)
{
    if (dev->ops->flush == NULL)
        return 0; // No flush needed

    return dev->ops->flush(dev);
}

long block_read_bytes(block_device_t *dev, void *buffer, size_t count, uint64_t offset)
{
    uint64_t size = dev->sector_count * BLOCK_SECTOR_SIZE;
    if (offset >= size)
        return 0;

    count = MIN(count, size - offset);

    uint8_t *out = (uint8_t *)buffer;
    size_t done = 0;
    while (done < count)
    {
        uint64_t position = offset + done;
        uint64_t index = position / BLOCK_CACHE_BLOCK_SIZE;
        size_t within = position % BLOCK_CACHE_BLOCK_SIZE;
        size_t chunk = MIN(count - done, BLOCK_CACHE_BLOCK_SIZE - within);

        block_cache_buf_t *buf;
        int ret = block_cache_get(dev, index, &buf);
        if (ret < 0)
            return ret;

        memcpy(out + done, buf->data + within, chunk);
        block_cache_put(buf);
        done += chunk;
    }

    return (long)done;
}

long block_write_bytes(block_device_t *dev, const void *buffer, size_t count, uint64_t offset)
{
    if (count == 0)
        return 0;

    uint64_t size = dev->sector_count * BLOCK_SECTOR_SIZE;
    if (offset >= size)
        return -ENOSPC;

    count = MIN(count, size - offset);

    const uint8_t *in = (const uint8_t *)buffer;
    size_t done = 0;
    while (done < count)
    {
        uint64_t position = offset + done;
        uint64_t index = position / BLOCK_CACHE_BLOCK_SIZE;
        size_t within = position % BLOCK_CACHE_BLOCK_SIZE;
        size_t chunk = MIN(count - done, BLOCK_CACHE_BLOCK_SIZE - within);

        block_cache_buf_t *buf;
        int ret = block_cache_get(dev, index, &buf);
        if (ret < 0)
            return ret;

        block_cache_begin_write(buf);
        memcpy(buf->data + within, in + done, chunk);
        block_cache_end_write(buf);
        block_cache_put(buf);
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