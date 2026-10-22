#include <driver/block_cache.h>
#include <mm/kmalloc.h>
#include <mihos/errno.h>
#include <string.h>
#include <panic.h>
#include <util.h>

static block_cache_buf_t block_cache_buffers[BLOCK_CACHE_BUFFERS];
static hlist_head_t block_cache_hash[HASH_BUCKETS(BLOCK_CACHE_HASH_BITS)]; // zeroed (static): empty
static list_t block_cache_lru;
static spinlock_t block_cache_lock;
static uint64_t block_cache_hits;
static uint64_t block_cache_misses;

/**
 * @brief Computes the hash bucket for a given device and block index.
 *
 * @param dev The block device.
 * @param index The block index within the device.
 * @return Pointer to the hash bucket.
 */
static hlist_head_t *block_cache_bucket(block_device_t *dev, uint64_t index)
{
    return &block_cache_hash[hash_64((uint64_t)(uintptr_t)dev ^ index, BLOCK_CACHE_HASH_BITS)];
}

/**
 * @brief Computes how many sectors are in a block, considering the disk's end.
 *
 * @param dev The block device.
 * @param index The block index within the device.
 * @return Number of sectors in the block.
 */
static uint32_t block_cache_sectors(block_device_t *dev, uint64_t index)
{
    return (uint32_t)MIN((uint64_t)BLOCK_CACHE_SECTORS, dev->sector_count - index * BLOCK_CACHE_SECTORS);
}

#define BLOCK_CACHE_WRITE_BACK 1 // block_cache_claim: *out is a dirty buffer to write back first

/**
 * @brief Finds (dev, index) or claims the least recently used buffer nobody holds for
 * it, with one reference taken. Only list and key work, never I/O: the lock is
 * a spinlock.
 *
 * @param dev The block device.
 * @param index The block index within the device.
 * @param out The output pointer to the claimed buffer.
 * @return 0 on success, BLOCK_CACHE_WRITE_BACK if a dirty buffer must be written back first, -ENOBUFS if no buffer could be claimed.
 */
static int block_cache_claim(block_device_t *dev, uint64_t index, block_cache_buf_t **out)
{
    guard(spinlock, &block_cache_lock);

    hlist_head_t *bucket = block_cache_bucket(dev, index);
    hlist_node_t *node;
    hlist_for_each(node, bucket)
    {
        block_cache_buf_t *buf = hlist_container(node, block_cache_buf_t, hash_link);

        if (buf->dev == dev && buf->index == index)
        {
            buf->refs++;
            block_cache_hits++;
            *out = buf;
            return 0;
        }
    }

    // Not cached: from the back of the LRU list, the oldest buffer nobody holds
    for (list_t *pos = block_cache_lru.prev; pos != &block_cache_lru; pos = pos->prev)
    {
        block_cache_buf_t *buf = list_container(pos, block_cache_buf_t, lru_link);
        if (buf->refs != 0)
            continue; // skip buffers that are currently held

        // Dirty: its block must reach the disk before the buffer can hold another one.
        // Pin it under its old key (a get of that block still finds it) and let the
        // caller write it back without this spinlock
        if (buf->dirty)
        {
            buf->refs = 1;
            *out = buf;
            return BLOCK_CACHE_WRITE_BACK;
        }

        // Remove the buffer from its current hash bucket if it is linked.
        if (hlist_linked(&buf->hash_link))
            hlist_remove(&buf->hash_link);

        buf->dev = dev;
        buf->index = index;
        buf->refs = 1;
        buf->valid = false;
        hlist_insert(bucket, &buf->hash_link);
        block_cache_misses++;
        *out = buf;
        return 0;
    }

    return -ENOBUFS;
}

/**
 * @brief Writes back a dirty buffer to its block device.
 *
 * @param buf The buffer to write back.
 * @return 0 on success, or a negative error code on failure.
 */
static int block_cache_write_back(block_cache_buf_t *buf)
{
    guard(mutex, &buf->data_lock);

    if (!buf->dirty)
        return 0;

    int ret = block_write(buf->dev, buf->index * BLOCK_CACHE_SECTORS, block_cache_sectors(buf->dev, buf->index), buf->data);
    if (ret == 0)
        buf->dirty = false;

    return ret;
}

/**
 * @brief Unpins a buffer, decrementing its reference count.
 *
 * @param buf The buffer to unpin.
 */
static void block_cache_unpin(block_cache_buf_t *buf)
{
    guard(spinlock, &block_cache_lock);

    if (buf->refs == 0)
        panic("block_cache_unpin: buffer has no references\n");

    buf->refs--;
}

void block_cache_init(void)
{
    spinlock_init(&block_cache_lock);
    list_init(&block_cache_lru);

    for (int i = 0; i < BLOCK_CACHE_BUFFERS; i++)
    {
        block_cache_buf_t *buf = &block_cache_buffers[i];

        uint8_t *data = kmalloc(BLOCK_CACHE_BLOCK_SIZE);
        if (data == NULL)
            panic("block_cache_init: failed to allocate buffer\n");

        list_init(&buf->lru_link);
        mutex_init(&buf->data_lock);
        hlist_node_init(&buf->hash_link);
        buf->dev = NULL;
        buf->refs = 0;
        buf->valid = false;
        buf->data = data;

        list_insert_before(&block_cache_lru, &buf->lru_link);
    }
}

int block_cache_get(block_device_t *dev, uint64_t index, block_cache_buf_t **out)
{
    uint64_t blocks = (dev->sector_count + BLOCK_CACHE_SECTORS - 1) / BLOCK_CACHE_SECTORS;
    if (index >= blocks)
        return -EINVAL;

    block_cache_buf_t *buf;
    int ret;
    while ((ret = block_cache_claim(dev, index, &buf)) == BLOCK_CACHE_WRITE_BACK)
    {
        // The oldest free buffer holds unsaved data: save it, then claim again
        ret = block_cache_write_back(buf);
        block_cache_unpin(buf);
        if (ret < 0)
            return ret;
    }
    if (ret < 0)
        return ret;

    // Read the block unless it is there; a second reader of the same block waits here
    scoped_guard(mutex, &buf->data_lock)
    {
        if (!buf->valid)
        {
            uint64_t first = index * BLOCK_CACHE_SECTORS;
            uint32_t sectors = block_cache_sectors(dev, index);

            // The disk's last block may be partial: what lies past the disk reads as zeros
            if (sectors < BLOCK_CACHE_SECTORS)
                memset(buf->data + sectors * BLOCK_SECTOR_SIZE, 0, (BLOCK_CACHE_SECTORS - sectors) * BLOCK_SECTOR_SIZE);

            ret = block_read(dev, first, sectors, buf->data);
            buf->valid = ret == 0;
        }
    }

    if (ret < 0)
    {
        block_cache_put(buf);
        return ret;
    }

    *out = buf;
    return 0;
}

void block_cache_put(block_cache_buf_t *buf)
{
    scoped_guard(spinlock, &block_cache_lock)
    {
        if (buf->refs == 0)
            panic("block_cache_put: buffer has no references\n");

        // Move the buffer to the end of the LRU list if it has no more references
        if (--buf->refs == 0)
            list_move_after(&block_cache_lru, &buf->lru_link);
    }
}

void block_cache_begin_write(block_cache_buf_t *buf)
{
    mutex_lock(&buf->data_lock);
}

void block_cache_end_write(block_cache_buf_t *buf)
{
    buf->dirty = true;
    mutex_unlock(&buf->data_lock);
}

void block_cache_stats(uint64_t *hits, uint64_t *misses)
{
    scoped_guard(spinlock, &block_cache_lock)
    {
        *hits = block_cache_hits;
        *misses = block_cache_misses;
    }
}