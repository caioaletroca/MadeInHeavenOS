#include "internal.h"
#include <mihos/errno.h>
#include <string.h>
#include <util.h>

// ---- Disk access: every read and write of the file system's blocks goes through these ----

/**
 * @brief Get a block from the ext2 filesystem
 *
 * @param volume: The ext2 filesystem
 * @param block: The block number to get
 * @param lease: Pointer to a block cache buffer lease
 * @param data: Pointer to store the data pointer
 * @return 0 on success, negative error code on failure. The block data is returned via the @data parameter.
 */
int ext2_block_get(ext2_volume_t *volume, uint32_t block, block_cache_buf_t **lease, const uint8_t **data)
{
    if (block == 0 || block >= volume->blocks_count)
        return -EIO; // a corrupt pointer: trusting it would read some other file's data

    uint64_t byte = (uint64_t)block * volume->block_size;
    int ret = block_cache_get(volume->disk, byte / BLOCK_CACHE_BLOCK_SIZE, lease);
    if (ret < 0)
        return ret;

    *data = (*lease)->data + byte % BLOCK_CACHE_BLOCK_SIZE;
    return 0;
}

/**
 * @brief Copy bytes into an ext2 block (through the block cache: on the disk at eviction or sync)
 *
 * @param volume: The ext2 filesystem
 * @param block: The block number to write
 * @param within: The byte offset inside the block
 * @param data: The bytes to copy
 * @param count: How many (within + count must not pass the block's end)
 * @return 0 on success, negative error code on failure
 */
int ext2_block_write(ext2_volume_t *volume, uint32_t block, size_t within, const void *data, size_t count)
{
    block_cache_buf_t *lease;
    const uint8_t *start;
    int ret = ext2_block_get(volume, block, &lease, &start);
    if (ret < 0)
        return ret;

    block_cache_begin_write(lease);
    memcpy(lease->data + (start - lease->data) + within, data, count);
    block_cache_end_write(lease);
    block_cache_put(lease);
    return 0;
}

/**
 * @brief Zero out a block on disk
 *
 * @param volume: The ext2 volume
 * @param block: The block number to zero
 * @param within: The starting offset within the block
 * @param count: The number of bytes to zero within the block
 * @return int: 0 on success, negative error code on failure
 */
int ext2_block_zero(ext2_volume_t *volume, uint32_t block, size_t within, size_t count)
{
    block_cache_buf_t *lease;
    const uint8_t *start;
    int ret = ext2_block_get(volume, block, &lease, &start);
    if (ret < 0)
        return ret;

    block_cache_begin_write(lease);
    memset(lease->data + (start - lease->data) + within, 0, count);
    block_cache_end_write(lease);
    block_cache_put(lease);
    return 0;
}

/**
 * @brief Write bytes anywhere in the file system, through the block cache
 *
 * For metadata that is not a whole block or lies outside the data blocks: the
 * superblock (in block 0 when blocks are 4 KiB, which ext2_block_get refuses),
 * group descriptors, inode fields.
 *
 * @param volume: The ext2 filesystem
 * @param byte: The byte offset on the disk
 * @param data: The bytes to write
 * @param count: How many
 * @return 0 on success, negative error code on failure
 */
int ext2_disk_write(ext2_volume_t *volume, uint64_t byte, const void *data, size_t count)
{
    long ret = block_write_bytes(volume->disk, data, count, byte);
    if (ret < 0)
        return ret;
    if (ret != (long)count)
        return -EIO;
    return 0;
}

/**
 * @brief Zero out a range of bytes on disk
 *
 * @param volume: The ext2 volume
 * @param byte: The starting byte offset
 * @param count: The number of bytes to zero
 * @return int: 0 on success, negative error code on failure
 */
int ext2_disk_zero(ext2_volume_t *volume, uint64_t byte, size_t count)
{
    static const uint8_t zeros[128];
    while (count > 0)
    {
        size_t chunk = MIN(count, sizeof(zeros));
        int ret = ext2_disk_write(volume, byte, zeros, chunk);
        if (ret < 0)
            return ret;
        byte += chunk;
        count -= chunk;
    }
    return 0;
}
