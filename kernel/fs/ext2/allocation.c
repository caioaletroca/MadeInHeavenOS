#include "internal.h"
#include <mihos/errno.h>
#include <util.h>

// ---- Block allocation: the groups' block bitmaps and the free counts, under allocation_lock ----

/**
 * @brief Find a free bit in a bitmap
 *
 * @param volume: The ext2 volume
 * @param bitmap: The block number of the bitmap
 * @param from: The starting bit to search from
 * @param bits: The group's size: bits at and after it are not searched
 * @return long: The index of the first free bit, or negative error code if none found
 */
static long ext2_bitmap_find(ext2_volume_t *volume, uint32_t bitmap, uint32_t from, uint32_t bits)
{
    block_cache_buf_t *lease;
    const uint8_t *data;
    int ret = ext2_block_get(volume, bitmap, &lease, &data);
    if (ret < 0)
        return ret;

    long found = -ENOSPC;
    for (uint32_t bit = from; bit < bits; bit++)
    {
        if ((data[bit / 8] & (1u << (bit % 8))) == 0)
        {
            found = bit;
            break;
        }
    }

    block_cache_put(lease);
    return found;
}

/**
 * @brief Set or clear a bit in a bitmap
 *
 * @param volume: The ext2 volume
 * @param bitmap: The block number of the bitmap
 * @param bit: The bit index to set or clear
 * @param used: True to set the bit, false to clear it
 * @return int: 0 on success, negative error code on failure
 */
static int ext2_bitmap_set(ext2_volume_t *volume, uint32_t bitmap, uint32_t bit, bool used)
{
    block_cache_buf_t *lease;
    const uint8_t *data;
    int ret = ext2_block_get(volume, bitmap, &lease, &data);
    if (ret < 0)
        return ret;

    uint8_t byte = data[bit / 8];
    block_cache_put(lease);

    uint8_t mask = (uint8_t)(1u << (bit % 8));
    if (((byte & mask) != 0) == used)
        return -EIO;

    byte = used ? (byte | mask) : (byte & ~mask);
    return ext2_block_write(volume, bitmap, bit / 8, &byte, 1);
}

/**
 * @brief Store a group descriptor to disk
 *
 * @param volume: The ext2 volume
 * @param group: The group index
 * @return int: 0 on success, negative error code on failure
 */
static int ext2_group_store(ext2_volume_t *volume, uint32_t group)
{
    uint64_t table = (uint64_t)(volume->first_data_block + 1) * volume->block_size;
    return ext2_disk_write(volume, table + (uint64_t)group * sizeof(ext2_group_t), &volume->group_table[group], sizeof(ext2_group_t));
}

/**
 * @brief Store the free block and inode counts to the superblock
 *
 * @param volume: The ext2 volume
 * @return int: 0 on success, negative error code on failure
 */
static int ext2_super_store_counts(ext2_volume_t *volume)
{
    uint32_t counts[2] = {volume->free_blocks, volume->free_inodes};
    return ext2_disk_write(volume, EXT2_SUPERBLOCK_OFFSET + offsetof(ext2_super_t, s_free_blocks_count), counts, sizeof(counts));
}

/**
 * @brief Allocate a free block
 *
 * @param volume: The ext2 volume
 * @param goal: The preferred block number
 * @param out: Pointer to store the allocated block number
 * @return int: 0 on success, negative error code on failure
 */
int ext2_block_alloc(ext2_volume_t *volume, uint32_t goal, uint32_t *out)
{
    guard(mutex, &volume->allocation_lock);

    if (volume->free_blocks == 0)
        return -ENOSPC;

    if (goal < volume->first_data_block || goal >= volume->blocks_count)
        goal = volume->first_data_block;

    uint32_t start_group = (goal - volume->first_data_block) / volume->blocks_per_group;
    uint32_t start_bit = (goal - volume->first_data_block) % volume->blocks_per_group;

    for (uint32_t i = 0; i <= volume->groups; i++)
    {
        uint32_t block = 0;
        uint32_t g = (start_group + i) % volume->groups;
        uint32_t from = i == 0 ? start_bit : 0;

        if (volume->group_table[g].bg_free_blocks_count == 0)
            continue;

        uint32_t bits = MIN(volume->blocks_per_group, volume->blocks_count - volume->first_data_block - g * volume->blocks_per_group);

        long bit = ext2_bitmap_find(volume, volume->group_table[g].bg_block_bitmap, from, bits);
        if (bit == -ENOSPC)
            continue;
        else if (bit < 0)
            return bit;

        int ret = ext2_bitmap_set(volume, volume->group_table[g].bg_block_bitmap, bit, true);
        if (ret == 0)
        {
            volume->group_table[g].bg_free_blocks_count--;
            volume->free_blocks--;
        }
        if (ret == 0)
            ret = ext2_group_store(volume, g);
        if (ret == 0)
            ret = ext2_super_store_counts(volume);
        if (ret == 0)
        {
            block = volume->first_data_block + g * volume->blocks_per_group + bit;
            ret = ext2_block_zero(volume, block, 0, volume->block_size);
        }

        if (ret < 0)
            return ret;

        *out = block;
        return 0;
    }

    return -EIO;
}

/**
 * @brief Free a block
 *
 * @param volume: The ext2 volume
 * @param block: The block number to free
 * @return int: 0 on success, negative error code on failure
 */
int ext2_block_free(ext2_volume_t *volume, uint32_t block)
{
    guard(mutex, &volume->allocation_lock);

    if (block < volume->first_data_block || block >= volume->blocks_count)
        return -EIO;

    uint32_t g = (block - volume->first_data_block) / volume->blocks_per_group;
    uint32_t bit = (block - volume->first_data_block) % volume->blocks_per_group;

    int ret = ext2_bitmap_set(volume, volume->group_table[g].bg_block_bitmap, bit, false);
    if (ret == 0)
    {
        volume->group_table[g].bg_free_blocks_count++;
        volume->free_blocks++;
        ret = ext2_group_store(volume, g);
    }
    if (ret == 0)
        ret = ext2_super_store_counts(volume);

    return ret;
}

// ---- Inode allocation: the groups' inode bitmaps, new inodes' slots, under allocation_lock ----

/**
 * @brief Take a free inode: in the parent's group first, then the others
 *
 * Every group is searched once from its start (inodes have a goal group, not a goal bit);
 * group 0 from s_first_ino, so a reserved inode is never given out even if its bit is clear.
 * Only the bitmap and the counts change: ext2_inode_init prepares the slot.
 *
 * @param volume: The ext2 filesystem
 * @param parent: The directory the new inode will be linked into
 * @param directory: The new inode is a directory (counted in bg_used_dirs_count)
 * @param out: Receives the inode number
 * @return 0, -ENOSPC when no inode is free, -EIO if the counters and bitmaps disagree, or the disk's error
 */
int ext2_inode_alloc(ext2_volume_t *volume, uint32_t parent, bool directory, uint32_t *out)
{
    guard(mutex, &volume->allocation_lock);

    if (volume->free_inodes == 0)
        return -ENOSPC;

    uint32_t start = (parent - 1) / volume->inodes_per_group;

    for (uint32_t i = 0; i < volume->groups; i++)
    {
        uint32_t g = (start + i) % volume->groups;
        uint32_t from = g == 0 ? volume->first_ino - 1 : 0; // inodes 1..first_ino-1 are reserved

        if (volume->group_table[g].bg_free_inodes_count == 0)
            continue;

        uint32_t bits = MIN(volume->inodes_per_group, volume->inodes_count - g * volume->inodes_per_group);

        long bit = ext2_bitmap_find(volume, volume->group_table[g].bg_inode_bitmap, from, bits);
        if (bit == -ENOSPC)
            continue;
        else if (bit < 0)
            return bit;

        int ret = ext2_bitmap_set(volume, volume->group_table[g].bg_inode_bitmap, (uint32_t)bit, true);
        if (ret == 0)
        {
            volume->group_table[g].bg_free_inodes_count--;
            volume->free_inodes--;
            if (directory)
                volume->group_table[g].bg_used_dirs_count++;

            ret = ext2_group_store(volume, g);
        }
        if (ret == 0)
            ret = ext2_super_store_counts(volume);

        if (ret < 0)
            return ret;

        *out = g * volume->inodes_per_group + (uint32_t)bit + 1; // inodes count from 1, bits from 0

        return 0;
    }

    return -EIO; // free_inodes said some were free, but no bitmap has one
}

/**
 * @brief Make a freshly allocated inode's slot a new, empty inode: everything zeroed,
 * then the type and permissions, the link count, the times, and the extra area's size
 *
 * The whole slot is zeroed: a reused one still holds its last owner's block map, i_dtime,
 * and past byte 128 its extended attributes. Size, block map and i_blocks stay 0.
 *
 * @param volume: The ext2 filesystem
 * @param ino: The inode (from ext2_inode_alloc)
 * @param mode: EXT2_S_IF* | permissions
 * @param links: i_links_count (1 for a file, 2 for a directory: its name and its ".")
 * @param time: All three times (no clock: the caller passes its parent's mtime)
 * @return 0 on success, negative error code on failure
 */
int ext2_inode_init(ext2_volume_t *volume, uint32_t ino, uint16_t mode, uint16_t links, uint32_t time)
{
    uint64_t byte = ext2_inode_byte(volume, ino);

    int ret = ext2_disk_zero(volume, byte, volume->inode_size);

    if (ret == 0)
    {
        ext2_inode_t inode = {
            .i_mode = mode,
            .i_links_count = links,
            .i_mtime = time,
            .i_atime = time,
            .i_ctime = time};

        ret = ext2_disk_write(volume, byte, &inode, sizeof(inode));
    }
    // Large inodes: how much of the area after byte 128 is valid (all zeros here: no extra times, no crtime)
    if (ret == 0 && volume->want_extra_isize != 0)
        ret = ext2_disk_write(volume, byte + EXT2_INODE_EXTRA_ISIZE, &volume->want_extra_isize, sizeof(volume->want_extra_isize));

    return ret;
}

/**
 * @brief Give an inode back: marked deleted in its slot (i_dtime, and i_links_count 0), then
 * its bit and the counts
 *
 * The caller has freed its blocks (truncate to 0). The slot is written first, outside the
 * lock (nobody else writes it): a deleted inode still marked allocated is a leak, never two
 * owners. Its links are cleared here, whatever the caller left: a create or mkdir failing
 * after ext2_inode_init gives back a slot that still says 1 or 2.
 *
 * @param volume: The ext2 filesystem
 * @param ino: The inode
 * @param directory: It was a directory (bg_used_dirs_count)
 * @return 0, -EIO for a reserved or out-of-range inode or one already free, or the disk's error
 */
int ext2_inode_free(ext2_volume_t *volume, uint32_t ino, bool directory)
{
    if (ino < volume->first_ino || ino > volume->inodes_count)
        return -EIO;

    uint64_t byte = ext2_inode_byte(volume, ino);

    uint32_t dtime = volume->dtime; // "deleted" for e2fsck (0 would be an orphan); no clock, so the superblock's last write time
    int ret = ext2_disk_write(volume, byte + offsetof(ext2_inode_t, i_dtime), &dtime, sizeof(dtime));

    // A freed inode has no links: e2fsck calls an inode with links in use, whatever its bit says
    // (a create or mkdir failing after ext2_inode_init gives one back with 1 or 2)
    uint16_t links = 0;
    if (ret == 0)
        ret = ext2_disk_write(volume, byte + offsetof(ext2_inode_t, i_links_count), &links, sizeof(links));
    if (ret < 0)
        return ret;

    scoped_guard(mutex, &volume->allocation_lock)
    {
        uint32_t bit = (ino - 1) % volume->inodes_per_group;
        uint32_t g = (ino - 1) / volume->inodes_per_group;

        ret = ext2_bitmap_set(volume, volume->group_table[g].bg_inode_bitmap, bit, false);
        if (ret == 0)
        {
            volume->group_table[g].bg_free_inodes_count++;
            volume->free_inodes++;

            if (directory)
                volume->group_table[g].bg_used_dirs_count--;
        }
        if (ret == 0)
            ret = ext2_group_store(volume, g);
        if (ret == 0)
            ret = ext2_super_store_counts(volume);
    }

    if (ret < 0)
        return ret;

    return 0;
}
