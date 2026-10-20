#include <driver/bcache.h>
#include <mm/kmalloc.h>
#include <mihos/errno.h>
#include <string.h>
#include <panic.h>
#include <util.h>

static bcache_buf_t bcache_buffers[BCACHE_BUFFERS];
static hlist_head_t bcache_hash[HASH_BUCKETS(BCACHE_HASH_BITS)]; // zeroed (static): empty
static list_t bcache_lru;
static spinlock_t bcache_lock;
static uint64_t bcache_hits;
static uint64_t bcache_misses;

/**
 * @brief Computes the hash bucket for a given device and block index.
 *
 * @param dev The block device.
 * @param index The block index within the device.
 * @return Pointer to the hash bucket.
 */
static hlist_head_t *bcache_bucket(block_device_t *dev, uint64_t index)
{
    return &bcache_hash[hash_64((uint64_t)(uintptr_t)dev ^ index, BCACHE_HASH_BITS)];
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
static int bcache_claim(block_device_t *dev, uint64_t index, bcache_buf_t **out)
{
    guard(spinlock, &bcache_lock);

    hlist_head_t *bucket = bcache_bucket(dev, index);
    hlist_node_t *node;
    hlist_for_each(node, bucket)
    {
        bcache_buf_t *cache = hlist_container(node, bcache_buf_t, hash);

        if (cache->dev == dev && cache->index == index)
        {
            cache->refs++;
            bcache_hits++;
            *out = cache;
            return 0;
        }
    }

    // Not cached: from the back of the LRU list, the oldest buffer nobody holds
    for (list_t *pos = bcache_lru.prev; pos != &bcache_lru; pos = pos->prev)
    {
        bcache_buf_t *cache = list_container(pos, bcache_buf_t, lru);
        if (cache->refs != 0)
            continue; // skip buffers that are currently held

        if (hlist_hashed(&cache->hash))
            hlist_del(&cache->hash);

        cache->dev = dev;
        cache->index = index;
        cache->refs = 1;
        cache->valid = false;
        hlist_add_head(bucket, &cache->hash);
        bcache_misses++;
        *out = cache;
        return 0;
    }

    return -ENOBUFS;
}

void bcache_init(void)
{
    spinlock_init(&bcache_lock);
    list_init(&bcache_lru);

    for (int i = 0; i < BCACHE_BUFFERS; i++)
    {
        bcache_buf_t *cache = &bcache_buffers[i];

        uint8_t *data = kmalloc(BCACHE_BLOCK_SIZE);
        if (data == NULL)
            panic("bcache_init: failed to allocate buffer\n");

        list_init(&cache->lru);
        mutex_init(&cache->fill);
        hlist_node_init(&cache->hash);
        cache->dev = NULL;
        cache->refs = 0;
        cache->valid = false;
        cache->data = data;

        list_insert_before(&bcache_lru, &cache->lru);
    }
}

int bcache_get(block_device_t *dev, uint64_t index, bcache_buf_t **out)
{
    uint64_t blocks = (dev->sector_count + BCACHE_SECTORS - 1) / BCACHE_SECTORS;
    if (index >= blocks)
        return -EINVAL;

    bcache_buf_t *cache;
    int ret = bcache_claim(dev, index, &cache);
    if (ret < 0)
        return ret;

    // Read the block unless it is there; a second reader of the same block waits here
    scoped_guard(mutex, &cache->fill)
    {
        if (!cache->valid)
        {
            uint64_t first = index * BCACHE_SECTORS;
            uint32_t sectors = (uint32_t)MIN((uint64_t)BCACHE_SECTORS, dev->sector_count - first);

            // The disk's last block may be partial: what lies past the disk reads as zeros
            if (sectors < BCACHE_SECTORS)
                memset(cache->data + sectors * BLOCK_SECTOR_SIZE, 0, (BCACHE_SECTORS - sectors) * BLOCK_SECTOR_SIZE);

            ret = block_read(dev, first, sectors, cache->data);
            cache->valid = ret == 0;
        }
    }

    if (ret < 0)
    {
        bcache_put(cache);
        return ret;
    }

    *out = cache;
    return 0;
}

void bcache_put(bcache_buf_t *buf)
{
    scoped_guard(spinlock, &bcache_lock)
    {
        if (buf->refs == 0)
            panic("bcache_put: buffer has no references\n");

        // Move the buffer to the end of the LRU list if it has no more references
        if (--buf->refs == 0)
            list_move_after(&bcache_lru, &buf->lru);
    }
}

void bcache_status(uint64_t *hits, uint64_t *misses)
{
    scoped_guard(spinlock, &bcache_lock)
    {
        *hits = bcache_hits;
        *misses = bcache_misses;
    }
}