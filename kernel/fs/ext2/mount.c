#include "internal.h"
#include <mihos/errno.h>
#include <mm/kmalloc.h>
#include <string.h>
#include <kprintf.h>
#include <util.h>

// ---- Mounting: the superblock and the group table, checked; the root node; the boot mount ----

/**
 * @brief Read the superblock from the ext2 filesystem
 *
 * @param disk: The block device representing the ext2 filesystem
 * @param superblock: Pointer to store the superblock
 * @return 0 on success, negative error code on failure
 */
static int ext2_read_superblock(block_device_t *disk, ext2_super_t *superblock)
{
    block_cache_buf_t *lease;
    int ret = block_cache_get(disk, 0, &lease);
    if (ret < 0)
        return ret;

    memcpy(superblock, lease->data + EXT2_SUPERBLOCK_OFFSET, sizeof(ext2_super_t));
    block_cache_put(lease);
    return 0;
}

/**
 * @brief Check the validity of the ext2 superblock and initialize the filesystem structure
 *
 * @param superblock: The ext2 superblock
 * @param disk: The block device representing the ext2 filesystem
 * @param volume: The ext2 filesystem structure to initialize
 * @return 0 on success, negative error code on failure
 */
static int ext2_check_superblock(const ext2_super_t *superblock, const block_device_t *disk, ext2_volume_t *volume)
{
    if (superblock->s_magic != EXT2_MAGIC)
        return -EINVAL;

    // Revision 0 has no feature flags, so no FILETYPE: readdir would have to read every inode (debt)
    if (superblock->s_rev_level != EXT2_DYNAMIC_REV)
        return -EINVAL;

    // Unknown incompat features change how the disk must be read: refuse. Unknown ro_compat
    // ones only matter for writing (read_only below), and compat ones never do (Linux's rule)
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

    // A group's bitmaps are one block each: never more blocks or inodes than it has bits
    if (superblock->s_blocks_per_group > 8 * block_size || superblock->s_inodes_per_group > 8 * block_size)
        return -EIO;

    // Reserved inodes are below s_first_ino; the root (2) must be one of them, and some must be left
    if (superblock->s_first_ino <= EXT2_ROOT_INO || superblock->s_first_ino > superblock->s_inodes_count)
        return -EIO;

    // Groups cover the blocks after s_first_data_block (rounded up); together they must hold every inode
    uint32_t groups = (superblock->s_blocks_count - superblock->s_first_data_block + superblock->s_blocks_per_group - 1) / superblock->s_blocks_per_group;
    if ((uint64_t)groups * superblock->s_inodes_per_group < superblock->s_inodes_count)
        return -EIO;

    volume->block_size = block_size;
    volume->blocks_count = superblock->s_blocks_count;
    volume->first_data_block = superblock->s_first_data_block;
    volume->blocks_per_group = superblock->s_blocks_per_group;
    volume->inodes_count = superblock->s_inodes_count;
    volume->inodes_per_group = superblock->s_inodes_per_group;
    volume->inode_size = inode_size;
    volume->first_ino = superblock->s_first_ino;

    // i_dtime below s_inodes_count is an orphan list link to e2fsck, not a time
    volume->dtime = MAX(superblock->s_wtime, superblock->s_inodes_count);

    // Large inodes: new ones get s_want_extra_isize valid bytes after byte 128 (at least the minimum;
    // 32, Linux's default, when an old mke2fs left both 0). 128-byte inodes have no extra area
    volume->want_extra_isize = 0;
    if (inode_size > EXT2_GOOD_OLD_INODE_SIZE)
    {
        volume->want_extra_isize = MAX(superblock->s_want_extra_isize, superblock->s_min_extra_isize);
        if (volume->want_extra_isize == 0)
            volume->want_extra_isize = 32;
    }

    volume->groups = groups;
    volume->read_only = (superblock->s_feature_ro_compat & ~EXT2_FEATURE_RO_COMPAT_KNOWN) != 0 ||
                        disk->ops->write == NULL;

    uint64_t pointers = block_size / sizeof(uint32_t);
    uint64_t reach = (EXT2_DIRECT_BLOCKS + pointers + pointers * pointers + pointers * pointers * pointers) * block_size;
    if (superblock->s_feature_ro_compat & EXT2_FEATURE_RO_COMPAT_LARGE_FILE)
        volume->max_file_size = MIN(reach, (uint64_t)1 << 40);
    else
        volume->max_file_size = MIN(reach, ((uint64_t)1 << 31) - 1);

    return 0;
}

/**
 * @brief Read the group descriptors from the disk and initialize the group table
 *
 * @param volume: The ext2 filesystem structure
 * @param first_data_block: The first data block of the filesystem
 * @return 0 on success, negative error code on failure
 */
static int ext2_read_groups(ext2_volume_t *volume, uint32_t first_data_block)
{
    size_t size = (size_t)volume->groups * sizeof(ext2_group_t);
    volume->group_table = kmalloc(size);
    if (volume->group_table == NULL)
        return -ENOMEM;

    uint8_t *out = (uint8_t *)volume->group_table;
    uint32_t block = first_data_block + 1;
    for (size_t done = 0; done < size; block++)
    {
        block_cache_buf_t *lease;
        const uint8_t *data;
        int ret = ext2_block_get(volume, block, &lease, &data);
        if (ret < 0)
            return ret;

        size_t chunk = MIN((size_t)volume->block_size, size - done);
        memcpy(out + done, data, chunk);
        block_cache_put(lease);
        done += chunk;
    }

    // Every group's part of the inode table must lie inside the file system: ext2_node_get trusts these numbers
    uint64_t table_blocks = ((uint64_t)volume->inodes_per_group * volume->inode_size + volume->block_size - 1) / volume->block_size;
    for (uint32_t i = 0; i < volume->groups; i++)
    {
        uint32_t table = volume->group_table[i].bg_inode_table;
        if (table == 0 || table + table_blocks > volume->blocks_count)
            return -EIO;
    }

    return 0;
}

/**
 * @brief Free the memory allocated for the ext2 filesystem structure
 *
 * @param volume: The ext2 filesystem structure to free
 */
static void ext2_volume_free(ext2_volume_t *volume)
{
    kfree(volume->group_table);
    kfree(volume);
}

/**
 * @brief Store the state of the ext2 filesystem on disk
 *
 * @param volume: The ext2 filesystem structure
 * @param state: The state to store (EXT2_VALID_FS or EXT2_ERROR_FS)
 * @return 0 on success, negative error code on failure
 */
static int ext2_state_store(ext2_volume_t *volume, uint16_t state)
{
    volume->state = state;

    int ret = ext2_disk_write(volume, EXT2_SUPERBLOCK_OFFSET + offsetof(ext2_super_t, s_state), &state, sizeof(state));
    if (ret < 0)
        return ret;

    return 0;
}

/**
 * @brief Halt hook for ext2 volumes: marks the volume clean before power off
 *
 * @param hook: The halt hook structure
 */
static void ext2_halt(halt_hook_t *hook)
{
    ext2_volume_t *volume = list_container(hook, ext2_volume_t, halt_hook);
    int ret = ext2_state_store(volume, volume->state | EXT2_VALID_FS);
    if (ret < 0)
        kprintf("ext2: %s: cannot mark the volume clean: %d\n", volume->disk->name, ret);
}

int ext2_mount(block_device_t *disk, vfs_node_t **root)
{
    ext2_super_t superblock;
    int ret = ext2_read_superblock(disk, &superblock);
    if (ret < 0)
        return ret;

    ext2_volume_t *volume = kzalloc(sizeof(ext2_volume_t));
    if (volume == NULL)
        return -ENOMEM;

    volume->disk = disk;

    ret = ext2_check_superblock(&superblock, disk, volume);
    if (ret == 0)
        ret = ext2_read_groups(volume, superblock.s_first_data_block);
    if (ret < 0)
    {
        ext2_volume_free(volume);
        return ret;
    }

    mutex_init(&volume->allocation_lock);
    mutex_init(&volume->lock);
    volume->free_blocks = superblock.s_free_blocks_count;
    volume->free_inodes = superblock.s_free_inodes_count;
    volume->dev = vfs_dev_alloc();

    vfs_node_t *node;
    ret = ext2_node_get(volume, EXT2_ROOT_INO, NULL, &node);
    if (ret == 0 && node->type != VFS_DIRECTORY)
    {
        vfs_node_put(node);
        ret = -EIO;
    }
    if (ret < 0)
    {
        ext2_volume_free(volume);
        return ret;
    }

    volume->state = superblock.s_state;

    // If the volume is not read-only, check its state and mark it as not cleanly unmounted if necessary
    if (!volume->read_only)
    {
        if ((volume->state & EXT2_VALID_FS) == 0 || (volume->state & EXT2_ERROR_FS) != 0)
            kprintf("ext2: %s was not cleanly unmounted or has errors: run e2fsck\n", disk->name);

        // Mark the volume as not cleanly unmounted before continuing
        ret = ext2_state_store(volume, volume->state & ~EXT2_VALID_FS);
        if (ret < 0)
        {
            vfs_node_put(node);
            ext2_volume_free(volume);
            return ret;
        }

        volume->halt_hook.run = ext2_halt;
        halt_hook_add(&volume->halt_hook);
    }

    *root = node;
    return 0;
}

void ext2_init(const block_device_t *root_device)
{
    for (size_t i = 0; block_at(i) != NULL; i++)
    {
        block_device_t *disk = block_at(i);

        if (disk == root_device)
            continue;

        vfs_node_t *root;
        int ret = ext2_mount(disk, &root);
        if (ret == -EINVAL) // not ext2, or not one we can read: try the next disk
            continue;

        if (ret < 0)
        {
            kprintf("ext2: %s: cannot mount: %d\n", disk->name, ret);
            continue;
        }

        ret = vfs_mount("/mnt", root);
        vfs_node_put(root);

        if (ret < 0)
            kprintf("ext2: %s: cannot mount on /mnt: %d\n", disk->name, ret);
        else
            kprintf("ext2: %s mounted on /mnt\n", disk->name);
        return; // one disk on /mnt, mounted or not
    }

    kprintf("ext2: no disk to mount\n");
}
