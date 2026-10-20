#include <driver/block.h>
#include <string.h>
#include <mihos/errno.h>

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
