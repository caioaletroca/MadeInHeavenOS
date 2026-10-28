#include "internal.h"
#include <mihos/errno.h>

// ---- File blocks: a file's logical block number to its block on the disk, through the tables ----

/**
 * @brief The block a pointer slot holds, or with create a new (zeroed) one if it holds 0
 *
 * A slot is n->block[index] when table is 0, else entry index of the table block `table`.
 * A new block is linked into its slot at once and counted in n->blocks; the caller
 * stores the inode afterwards.
 *
 * @param volume: The ext2 filesystem
 * @param n: The node (its lock held when create is set)
 * @param table: The table block holding the slot, or 0 for the inode's own pointers
 * @param index: The slot's index
 * @param goal: Where to look for a new block; moved past every block seen (NULL: not tracked)
 * @param create: Allocate a block for an empty slot
 * @param out: Receives the block, 0 for a hole (create not set)
 * @return 0 on success, negative error code on failure
 */
static int ext2_slot(ext2_volume_t *volume, ext2_node_t *node, uint32_t table, uint32_t index, uint32_t *goal, bool create, uint32_t *out)
{
    uint32_t block;
    if (table == 0)
        block = node->block[index];
    else
    {
        block_cache_buf_t *lease;
        const uint8_t *data;
        int ret = ext2_block_get(volume, table, &lease, &data);
        if (ret < 0)
            return ret;

        block = ((const uint32_t *)data)[index];
        block_cache_put(lease);
    }

    if (block == 0 && create)
    {
        int ret = ext2_block_alloc(volume, goal != NULL ? *goal : 0, &block);
        if (ret < 0)
            return ret;

        if (table == 0)
            node->block[index] = block;
        else
        {
            ret = ext2_block_write(volume, table, index * sizeof(uint32_t), &block, sizeof(block));
            if (ret < 0)
            {
                ext2_block_free(volume, block);
                return ret;
            }
        }
        node->blocks += volume->block_size / 512;
    }

    if (goal != NULL && block != 0)
        *goal = block + 1;

    *out = block;
    return 0;
}

/**
 * @brief Map a logical block number to a physical block number in an ext2 file
 * @param volume: The ext2 filesystem structure
 * @param node: The ext2 node
 * @param logical: The logical block number
 * @param goal: The preferred physical block to allocate (if creating)
 * @param create: Whether to create the block if it doesn't exist
 * @param out: The resulting physical block number
 * @return 0 on success, negative error code on failure
 */
int ext2_file_block(ext2_volume_t *volume, ext2_node_t *node, uint64_t logical, uint32_t *goal, bool create, uint32_t *out)
{
    if (logical < EXT2_DIRECT_BLOCKS)
        return ext2_slot(volume, node, 0, (uint32_t)logical, goal, create, out);

    logical -= EXT2_DIRECT_BLOCKS;

    uint64_t per_block = volume->block_size / sizeof(uint32_t);
    uint64_t span = per_block;

    for (int levels = 1; levels <= 3; levels++, span *= per_block)
    {
        if (logical >= span)
        {
            logical -= span;
            continue;
        }

        uint32_t block;
        int ret = ext2_slot(volume, node, 0, EXT2_DIRECT_BLOCKS + levels - 1, goal, create, &block);
        if (ret < 0)
            return ret;

        for (uint64_t below = span / per_block; block != 0; below /= per_block)
        {
            ret = ext2_slot(volume, node, block, (uint32_t)(logical / below), goal, create, &block);
            if (ret < 0)
                return ret;

            logical %= below;
            if (below == 1)
                break;
        }

        *out = block;
        return 0;
    }

    return -EFBIG;
}

/**
 * @brief Truncate a branch of an ext2 file's block tree.
 *
 * The tree hangs from *slot: a data block when depth is 0, else a table `depth` levels
 * above the data blocks. It covers `span` logical blocks, counted from its own start.
 * Every data block from `from` on is freed, and every table left with no pointers (a
 * table whose blocks are all past the cut) is freed too, its *slot set to 0. *slot is
 * the caller's copy: n->block[i], or a table entry the caller writes back.
 *
 *
 * @param volume: The ext2 filesystem structure
 * @param node: The ext2 node
 * @param slot: Pointer to the block slot to truncate
 * @param depth: The depth of the branch (0 for direct blocks)
 * @param span: The number of logical blocks covered by this branch
 * @param from: The logical block number from which to start truncating
 * @return 0 on success, negative error code on failure
 */
int ext2_truncate_branch(ext2_volume_t *volume, ext2_node_t *node, uint32_t *slot, int depth, uint64_t span, uint64_t from)
{
    int ret;

    if (*slot == 0 || from >= span)
        return 0;

    uint32_t sectors = volume->block_size / 512;
    if (depth == 0)
    {
        ret = ext2_block_free(volume, *slot);
        *slot = 0;
        node->blocks -= sectors;
        return ret;
    }

    uint64_t per_block = volume->block_size / sizeof(uint32_t);
    uint64_t below = span / per_block;
    bool used = false;
    for (uint64_t i = 0; i < per_block; i++)
    {
        uint32_t entry;
        ret = ext2_slot(volume, node, *slot, (uint32_t)i, NULL, false, &entry);
        if (ret < 0)
            return ret;

        // Skip entries that are entirely before the truncation point
        uint64_t start = i * below;
        if (entry != 0 && from < start + below)
        {
            uint32_t before = entry;
            ret = ext2_truncate_branch(volume, node, &entry, depth - 1, below, from > start ? from - start : 0);
            if (entry != before)
            {
                int stored = ext2_block_write(volume, *slot, i * sizeof(uint32_t), &entry, sizeof(uint32_t));
                if (ret == 0)
                    ret = stored;
            }

            if (ret < 0)
                return ret;
        }

        used = used || entry != 0;
    }

    if (used)
        return 0;

    // Every pointer is 0 now: the table holds nothing, free it too
    ret = ext2_block_free(volume, *slot);
    *slot = 0;
    node->blocks -= sectors;
    return ret;
}
