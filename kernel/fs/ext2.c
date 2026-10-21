#include <fs/ext2.h>
#include "ext2_disk.h"
#include <driver/bcache.h>
#include <sched/sync.h>
#include <sys/hashtable.h>
#include <mihos/errno.h>
#include <mm/kmalloc.h>
#include <stdint.h>
#include <string.h>
#include <kprintf.h>
#include <util.h>

#define EXT2_NODE_HASH_BITS 6 // 64 buckets for cached nodes

typedef struct ext2_fs
{
    block_device_t *disk;
    uint64_t dev;          // st_dev (vfs_dev_alloc)
    uint32_t block_size;   // 1024, 2048 or 4096
    uint32_t blocks_count; // valid block numbers: 1 .. blocks_count - 1
    uint32_t inodes_count;
    uint32_t inodes_per_group;
    uint32_t inode_size; // bytes per inode in the table (128 or more)
    uint32_t groups;
    ext2_group_t *group_table; // kmalloc'd copy of the group descriptors
    mutex_t lock;              // the node cache: nodes[] and every node's hash link
    hlist_head_t nodes[HASH_BUCKETS(EXT2_NODE_HASH_BITS)];
} ext2_fs_t;

/**
 * @brief Get a block from the ext2 filesystem
 *
 * @param fs: The ext2 filesystem
 * @param block: The block number to get
 * @param lease: Pointer to a bcache buffer lease
 * @param data: Pointer to store the data pointer
 * @return 0 on success, negative error code on failure. The block data is returned via the @data parameter.
 */
static int ext2_block_get(ext2_fs_t *fs, uint32_t block, bcache_buf_t **lease, const uint8_t **data)
{
    if (block == 0 || block >= fs->blocks_count)
        return -EIO; // a corrupt pointer: trusting it would read some other file's data

    uint64_t byte = (uint64_t)block * fs->block_size;
    int ret = bcache_get(fs->disk, byte / BCACHE_BLOCK_SIZE, lease);
    if (ret < 0)
        return ret;

    *data = (*lease)->data + byte % BCACHE_BLOCK_SIZE;
    return 0;
}

/**
 * @brief Read the superblock from the ext2 filesystem
 *
 * @param disk: The block device representing the ext2 filesystem
 * @param superblock: Pointer to store the superblock
 * @return 0 on success, negative error code on failure
 */
static int ext2_read_super(block_device_t *disk, ext2_super_t *superblock)
{
    bcache_buf_t *lease;
    int ret = bcache_get(disk, 0, &lease);
    if (ret < 0)
        return ret;

    memcpy(superblock, lease->data + EXT2_SUPERBLOCK_OFFSET, sizeof(ext2_super_t));
    bcache_put(lease);
    return 0;
}

/**
 * @brief Check the validity of the ext2 superblock and initialize the filesystem structure
 *
 * @param superblock: The ext2 superblock
 * @param disk: The block device representing the ext2 filesystem
 * @param fs: The ext2 filesystem structure to initialize
 * @return 0 on success, negative error code on failure
 */
static int ext2_check_super(const ext2_super_t *superblock, const block_device_t *disk, ext2_fs_t *fs)
{
    if (superblock->s_magic != EXT2_MAGIC)
        return -EINVAL;

    // Revision 0 has no feature flags, so no FILETYPE: readdir would have to read every inode (debt)
    if (superblock->s_rev_level != EXT2_DYNAMIC_REV)
        return -EINVAL;

    // Unknown incompat features change how the disk must be read: refuse. Unknown ro_compat
    // ones only matter for writing, and compat ones never do (Linux's rule)
    if ((superblock->s_feature_incompat & ~EXT2_FEATURE_INCOMPAT_FILETYPE) != 0 ||
        (superblock->s_feature_incompat & EXT2_FEATURE_INCOMPAT_FILETYPE) == 0)
        return -EINVAL;

    // Bigger blocks would straddle two cache buffers
    if (superblock->s_log_block_size > 2)
        return -EINVAL;

    uint32_t block_size = 1024u << superblock->s_log_block_size;
    uint32_t inode_size = superblock->s_inode_size;

    if (inode_size < sizeof(ext2_inode_t) || (inode_size & (inode_size - 1)) != 0 || inode_size > block_size)
        return -EINVAL;

    if (superblock->s_blocks_per_group == 0 || superblock->s_inodes_per_group == 0 || superblock->s_first_data_block >= superblock->s_blocks_count)
        return -EIO;
    if ((uint64_t)superblock->s_blocks_count * block_size > disk->sector_count * BLOCK_SECTOR_SIZE)
        return -EIO;

    // Groups cover the blocks after s_first_data_block (rounded up); together they must hold every inode
    uint32_t groups = (superblock->s_blocks_count - superblock->s_first_data_block + superblock->s_blocks_per_group - 1) / superblock->s_blocks_per_group;
    if ((uint64_t)groups * superblock->s_inodes_per_group < superblock->s_inodes_count)
        return -EIO;

    fs->block_size = block_size;
    fs->blocks_count = superblock->s_blocks_count;
    fs->inodes_count = superblock->s_inodes_count;
    fs->inodes_per_group = superblock->s_inodes_per_group;
    fs->inode_size = inode_size;
    fs->groups = groups;

    return 0;
}

/**
 * @brief Read the group descriptors from the disk and initialize the group table
 *
 * @param fs: The ext2 filesystem structure
 * @param first_data_block: The first data block of the filesystem
 * @return 0 on success, negative error code on failure
 */
static int ext2_read_groups(ext2_fs_t *fs, uint32_t first_data_block)
{
    size_t size = (size_t)fs->groups * sizeof(ext2_group_t);
    fs->group_table = kmalloc(size);
    if (fs->group_table == NULL)
        return -ENOMEM;

    uint8_t *out = (uint8_t *)fs->group_table;
    uint32_t block = first_data_block + 1;
    for (size_t done = 0; done < size; block++)
    {
        bcache_buf_t *lease;
        const uint8_t *data;
        int ret = ext2_block_get(fs, block, &lease, &data);
        if (ret < 0)
            return ret;

        size_t chunk = MIN((size_t)fs->block_size, size - done);
        memcpy(out + done, data, chunk);
        bcache_put(lease);
        done += chunk;
    }

    // Every group's part of the inode table must lie inside the file system: iget trusts these numbers
    uint64_t table_blocks = ((uint64_t)fs->inodes_per_group * fs->inode_size + fs->block_size - 1) / fs->block_size;
    for (uint32_t i = 0; i < fs->groups; i++)
    {
        uint32_t table = fs->group_table[i].bg_inode_table;
        if (table == 0 || table + table_blocks > fs->blocks_count)
            return -EIO;
    }

    return 0;
}

/**
 * @brief Free the memory allocated for the ext2 filesystem structure
 *
 * @param fs: The ext2 filesystem structure to free
 */
static void ext2_fs_free(ext2_fs_t *fs)
{
    kfree(fs->group_table);
    kfree(fs);
}

int ext2_mount(block_device_t *disk, vfs_node_t **root)
{
    ext2_super_t superblock;
    int ret = ext2_read_super(disk, &superblock);
    if (ret < 0)
        return ret;

    ext2_fs_t *fs = kzalloc(sizeof(ext2_fs_t));
    if (fs == NULL)
        return -ENOMEM;

    fs->disk = disk;

    ret = ext2_check_super(&superblock, disk, fs);
    if (ret == 0)
        ret = ext2_read_groups(fs, superblock.s_first_data_block);
    if (ret < 0)
    {
        ext2_fs_free(fs);
        return ret;
    }

    mutex_init(&fs->lock);
    fs->dev = vfs_dev_alloc();

    // The root directory, inode 2, comes with the node cache (ext2_iget)
    (void)root;
    ext2_fs_free(fs);
    return -ENOSYS;
}
