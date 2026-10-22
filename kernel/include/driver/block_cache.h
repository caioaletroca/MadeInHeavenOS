#ifndef _DRIVER_BLOCK_CACHE_H_
#define _DRIVER_BLOCK_CACHE_H_

#include <stdbool.h>
#include <stdint.h>
#include <asm/memory.h>
#include <driver/block.h>
#include <sched/sync.h>
#include <sys/hashtable.h>
#include <sys/list.h>

#define BLOCK_CACHE_BLOCK_SIZE PAGE_SIZE                                 // one page: 1, 2 and 4 KiB ext2 blocks are slices of it
#define BLOCK_CACHE_SECTORS (BLOCK_CACHE_BLOCK_SIZE / BLOCK_SECTOR_SIZE) // 8
#define BLOCK_CACHE_BUFFERS 64                                           // 256 KiB of disk in RAM
#define BLOCK_CACHE_HASH_BITS 4                                          // 16 buckets

/**
 * One cached block of a disk. A caller holding a reference (a lease from
 * block_cache_get) may read or write data until block_cache_put; a buffer with no references can
 * be reused for another block at any time.
 */
typedef struct block_cache_buf
{
    block_device_t *dev;
    uint64_t index;
    unsigned int refs;
    list_t lru_link;
    hlist_node_t hash_link;
    mutex_t data_lock; // data, valid and dirty: held for a fill, a write-back and between begin_write and end_write
    bool valid;
    bool dirty; // data differs from the disk; only a holder sets it (end_write), only a write-back clears it
    uint8_t *data;
} block_cache_buf_t;

/**
 * @brief Allocates the buffers (panics if it cannot). Once, after mm_init.
 */
void block_cache_init(void);

/**
 * @brief Gets a block of a disk, read from the disk only if it is not cached.
 *
 * @param dev The disk.
 * @param index The block number (BLOCK_CACHE_BLOCK_SIZE bytes each).
 * @param out Receives the buffer, with a reference the caller must block_cache_put.
 * @return 0, -EINVAL past the end of the disk, -ENOBUFS if every buffer is in use, or the disk's error.
 */
int block_cache_get(block_device_t *dev, uint64_t index, block_cache_buf_t **out);

/**
 * @brief Gives a buffer back; at no references it becomes the most recently used.
 *
 * @param buf The buffer to release.
 */
void block_cache_put(block_cache_buf_t *buf);

/**
 * @brief Starts changing a held buffer's data: waits until no fill or write-back is
 * inside it. Hold one buffer's write at a time (two would deadlock against
 * another writer holding them the other way round).
 *
 * @param buf A buffer the caller holds (from block_cache_get).
 */
void block_cache_begin_write(block_cache_buf_t *buf);

/**
 * @brief Ends the change begun by block_cache_begin_write: the buffer is dirty and
 * reaches the disk when it is evicted or at the next block_cache_sync.
 *
 * @param buf The same buffer.
 */
void block_cache_end_write(block_cache_buf_t *buf);

/**
 * @brief How many gets found their block (hits) and how many had to claim a buffer (misses).
 *
 * @param hits Pointer to store the number of cache hits.
 * @param misses Pointer to store the number of cache misses.
 */
void block_cache_stats(uint64_t *hits, uint64_t *misses);

#endif /* _DRIVER_BLOCK_CACHE_H_ */