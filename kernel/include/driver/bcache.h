#ifndef _DRIVER_BCACHE_H_
#define _DRIVER_BCACHE_H_

#include <stdbool.h>
#include <stdint.h>
#include <asm/memory.h>
#include <driver/block.h>
#include <sched/sync.h>
#include <sys/hashtable.h>
#include <sys/list.h>

#define BCACHE_BLOCK_SIZE PAGE_SIZE                            // one page: 1, 2 and 4 KiB ext2 blocks are slices of it
#define BCACHE_SECTORS (BCACHE_BLOCK_SIZE / BLOCK_SECTOR_SIZE) // 8
#define BCACHE_BUFFERS 64                                      // 256 KiB of disk in RAM
#define BCACHE_HASH_BITS 4                                     // 16 buckets

/**
 * One cached 4 KiB block of a disk. A caller holding a reference (a lease from
 * bcache_get) may read data until bcache_put; a buffer with no references can
 * be reused for another block at any time.
 */
typedef struct bcache_buf
{
    block_device_t *dev;
    uint64_t index;
    unsigned int refs;
    list_t lru;
    hlist_node_t hash;
    mutex_t fill;
    bool valid;
    uint8_t *data;
} bcache_buf_t;

/**
 * @brief Allocates the buffers (panics if it cannot). Once, after mm_init.
 */
void bcache_init(void);

/**
 * @brief Gets a block of a disk, read from the disk only if it is not cached.
 *
 * @param dev The disk.
 * @param index The block number (BCACHE_BLOCK_SIZE bytes each).
 * @param out Receives the buffer, with a reference the caller must bcache_put.
 * @return 0, -EINVAL past the end of the disk, -ENOBUFS if every buffer is in use, or the disk's error.
 */
int bcache_get(block_device_t *dev, uint64_t index, bcache_buf_t **out);

/**
 * @brief Gives a buffer back; at no references it becomes the most recently used.
 *
 * @param buf The buffer to release.
 */
void bcache_put(bcache_buf_t *buf);

/**
 * @brief How many gets found their block (hits) and how many had to claim a buffer (misses).
 *
 * @param hits Pointer to store the number of cache hits.
 * @param misses Pointer to store the number of cache misses.
 */
void bcache_status(uint64_t *hits, uint64_t *misses);

#endif /* _DRIVER_BCACHE_H_ */