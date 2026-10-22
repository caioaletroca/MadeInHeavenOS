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
 * @brief Finds (dev, index) or claims the least recently used buffer nobody holds for
 * it, with one reference taken. Only list and key work, never I/O: the lock is
 * a spinlock.
 *
 * @param dev The block device.
 * @param index The block index within the device.
 * @param out The output pointer to the claimed buffer.
 * @return 0 on success, -ENOBUFS if no buffer could be claimed.
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
        mutex_init(&buf->fill_lock);
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
    int ret = block_cache_claim(dev, index, &buf);
    if (ret < 0)
        return ret;

    // Read the block unless it is there; a second reader of the same block waits here
    scoped_guard(mutex, &buf->fill_lock)
    {
        if (!buf->valid)
        {
            uint64_t first = index * BLOCK_CACHE_SECTORS;
            uint32_t sectors = (uint32_t)MIN((uint64_t)BLOCK_CACHE_SECTORS, dev->sector_count - first);

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

void block_cache_stats(uint64_t *hits, uint64_t *misses)
{
    scoped_guard(spinlock, &block_cache_lock)
    {
        *hits = block_cache_hits;
        *misses = block_cache_misses;
    }
}