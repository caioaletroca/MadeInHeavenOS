#include "internal.h"
#include <mihos/errno.h>
#include <string.h>
#include <util.h>

// ---- File data: read, write and truncate of a file's bytes ----

/**
 * @brief Read data from an ext2 file
 *
 * @param file: The VFS node representing the file
 * @param buffer: The buffer to read data into
 * @param count: The number of bytes to read
 * @param offset: The offset within the file to start reading from
 * @return long: The number of bytes read, or a negative error code on failure
 */
long ext2_read(vfs_node_t *file, void *buffer, size_t count, uint64_t offset)
{
    ext2_node_t *n = EXT2_NODE(file);
    ext2_volume_t *volume = n->volume;

    guard(mutex, &n->lock);

    if (offset >= file->size)
        return 0;

    count = MIN(count, file->size - offset);

    uint8_t *out = buffer;
    size_t done = 0;
    while (done < count)
    {
        uint64_t position = offset + done;
        uint64_t logical = position / volume->block_size;
        size_t within = position % volume->block_size;
        size_t chunk = MIN(volume->block_size - within, count - done);

        uint32_t block;
        int ret = ext2_file_block(volume, n, logical, NULL, false, &block);
        if (ret < 0)
            return ret;

        if (block == 0)
            memset(out + done, 0, chunk);
        else
        {
            block_cache_buf_t *lease;
            const uint8_t *data;
            ret = ext2_block_get(volume, block, &lease, &data);
            if (ret < 0)
                return ret;

            memcpy(out + done, data + within, chunk);
            block_cache_put(lease);
        }

        done += chunk;
    }

    return (long)done;
}

/**
 * @brief Write data to an ext2 file
 *
 * @param file: The VFS node representing the file
 * @param buffer: The buffer containing data to write
 * @param count: The number of bytes to write
 * @param offset: The offset within the file to start writing to
 * @return long: The number of bytes written, or a negative error code on failure
 */
long ext2_write(vfs_node_t *file, const void *buffer, size_t count, uint64_t offset)
{
    ext2_node_t *n = EXT2_NODE(file);
    ext2_volume_t *volume = n->volume;

    guard(mutex, &n->lock);

    if (offset == VFS_APPEND)
        offset = file->size; // O_APPEND: the end, read under the lock, so another write can't move it now

    uint64_t old_size = file->size;
    uint32_t old_blocks = n->blocks;

    if (count == 0)
        return 0;
    if (offset >= volume->max_file_size)
        return -EFBIG; // past the largest file this file system can hold

    count = MIN(count, volume->max_file_size - offset);

    // Look for new blocks after the one before the write, else at the start of the inode's group
    uint32_t group = (n->ino - 1) / volume->inodes_per_group;
    uint32_t goal = volume->first_data_block + group * volume->blocks_per_group;
    uint64_t first = offset / volume->block_size;
    if (first > 0)
    {
        uint32_t previous;
        if (ext2_file_block(volume, n, first - 1, NULL, false, &previous) == 0 && previous != 0)
            goal = previous + 1;
    }

    const uint8_t *in = buffer;
    size_t done = 0;
    int ret = 0;
    while (done < count)
    {
        uint64_t position = offset + done;
        uint64_t logical = position / volume->block_size;
        size_t within = position % volume->block_size;
        size_t chunk = MIN(volume->block_size - within, count - done);

        uint32_t block;
        ret = ext2_file_block(volume, n, logical, &goal, true, &block);
        if (ret == 0 && block == 0)
            ret = -EIO; // create never leaves a hole
        if (ret < 0)
            break;

        ret = ext2_block_write(volume, block, within, in + done, chunk);
        if (ret < 0)
            break;

        done += chunk;
    }

    // What was written stays written: the size covers it, and blocks linked before an
    // error are in the inode (counted and consistent, only maybe unused)
    uint64_t end = offset + done;
    if (end > file->size)
        file->size = end;
    if (file->size != old_size || n->blocks != old_blocks)
    {
        int stored = ext2_inode_store(n);
        if (ret == 0)
            ret = stored;
    }

    return done > 0 ? (long)done : ret;
}

/**
 * Truncate a file to the specified size.
 * @param file The file to truncate.
 * @param size The new size of the file.
 * @return 0 on success, or a negative error code on failure.
 */
int ext2_truncate(vfs_node_t *file, uint64_t size)
{
    ext2_node_t *node = EXT2_NODE(file);
    ext2_volume_t *volume = node->volume;

    guard(mutex, &node->lock);

    // Check if the requested size exceeds the maximum file size
    if (size > volume->max_file_size)
        return -EFBIG;

    // If the new size is equal to the current size, nothing needs to be done.
    if (size == file->size)
        return 0;
    // If the new size is greater than the current size, extend the file.
    else if (size > file->size)
    {
        file->size = size;
        return ext2_inode_store(node);
    }

    // The block that will hold the new end keeps its first bytes: the rest must read as
    // zeros if the file grows again, not as what was there
    int ret = 0;
    size_t within = size % volume->block_size;
    if (within != 0)
    {
        uint32_t block;
        ret = ext2_file_block(volume, node, size / volume->block_size, NULL, false, &block);
        if (ret == 0 && block != 0)
            ret = ext2_block_zero(volume, block, within, volume->block_size - within);
    }

    // Free everything from the first block past the new end: direct pointers, then each tree
    uint64_t keep = (size + volume->block_size - 1) / volume->block_size;
    for (uint64_t i = keep; ret == 0 && i < EXT2_DIRECT_BLOCKS; i++)
        ret = ext2_truncate_branch(volume, node, &node->block[i], 0, 1, 0);

    uint64_t per_block = volume->block_size / sizeof(uint32_t);
    uint64_t start = EXT2_DIRECT_BLOCKS;
    uint64_t span = per_block;
    for (int levels = 1; ret == 0 && levels <= 3; levels++, span *= per_block)
    {
        uint64_t from = keep > start ? keep - start : 0;
        ret = ext2_truncate_branch(volume, node, &node->block[EXT2_DIRECT_BLOCKS + levels - 1], levels, span, from);
        start += span;
    }

    // Whatever happened, the inode must say what is freed: a block freed but still in i_block
    // would have two owners once it is allocated again
    file->size = size;
    int stored = ext2_inode_store(node);
    return ret != 0 ? ret : stored;
}
