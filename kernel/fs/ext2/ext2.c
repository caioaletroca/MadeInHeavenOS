#include "internal.h"
#include <mihos/errno.h>
#include <mm/kmalloc.h>
#include <string.h>
#include <kprintf.h>
#include <util.h>

// ---- What has not moved to its own file yet ----

/**
 * @brief A new, empty regular file in a directory: an inode taken and prepared, then its name
 *
 * The name comes last, so no record ever points at an unprepared inode; if it cannot be
 * added, the inode goes back. The directory's links and mtime stay (no "..", no clock).
 *
 * @param dir: The directory VFS node
 * @param name: The name of the new file (not '\0'-terminated)
 * @param length: The length of the name
 * @param mode: The permissions (07777)
 * @param child: Receives the new file's node, as a new reference
 * @return 0, -EEXIST if the name is taken (whatever its type), -EROFS for an indexed directory, or -errno
 */
int ext2_create(vfs_node_t *dir, const char *name, size_t length, uint32_t mode, vfs_node_t **child)
{
    ext2_node_t *node = EXT2_NODE(dir);
    ext2_volume_t *volume = node->volume;

    guard(mutex, &node->lock);

    if (node->flags & EXT2_INDEX_FL)
        return -EROFS; // a hashed directory: a record added without updating its index would hide from Linux's lookups

    ext2_lookup_context_t ctx = {
        .name = name,
        .length = length,
    };
    int ret = ext2_dir_walk(node, ext2_lookup_visit, &ctx);
    if (ret < 0)
        return ret;
    if (ret > 0)
        return -EEXIST; // whatever its type: a second record with the name would be corruption

    uint32_t ino = 0;
    ret = ext2_inode_alloc(volume, node->ino, false, &ino);
    if (ret < 0)
        return ret;

    ret = ext2_inode_init(volume, ino, EXT2_S_IFREG | (mode & 07777), 1, (uint32_t)dir->mtime);
    if (ret == 0)
        ret = ext2_dir_add(node, name, length, ino, EXT2_FT_REG_FILE);

    if (ret < 0)
    {
        ext2_inode_free(volume, ino, false);
        return ret;
    }

    return ext2_node_get(volume, ino, dir, child);
}

// ---- Deleting: names removed, and a node's blocks and inode freed at its last put ----

/**
 * @brief Release an ext2 node after its last reference: a file whose last name was removed
 * (free_on_release) gives back its blocks and inode first; then the node leaves the cache
 * and its parent is put (which may release it in turn)
 *
 * A failure to free can't be returned: it is logged and leaks (e2fsck mends a leak). Freeing
 * before the unhash is safe: a create that reuses the inode meanwhile finds this node at 0
 * references, unhashes it and reads the new inode.
 *
 * @param node: The VFS node to release
 */
void ext2_release(vfs_node_t *node)
{
    ext2_node_t *n = EXT2_NODE(node);

    // No names left and now no references: nobody can reach it again, give back its blocks and inode
    if (n->free_on_release)
    {
        int ret = ext2_truncate(node, 0);
        if (ret == 0)
            ret = ext2_inode_free(n->volume, n->ino, node->type == VFS_DIRECTORY);
        if (ret < 0)
            kprintf("ext2: freeing inode %u failed (%d): run e2fsck\n", n->ino, ret);
    }

    scoped_guard(mutex, &n->volume->lock)
    {
        if (hlist_linked(&n->cache_link))
            hlist_remove(&n->cache_link);
    }

    vfs_node_t *parent = node->parent != node ? node->parent : NULL;
    kfree(n);

    if (parent != NULL)
        vfs_node_put(parent);
}

/**
 * @brief Remove a regular file's name. The file itself stays while it is open: its
 * blocks and inode are freed at its node's last put (ext2_release)
 *
 * The node is read before the name goes, so a failure can never leave a file without a
 * name but with a link. Lock order: the directory's lock, then the file's.
 *
 * @param dir: The directory VFS node
 * @param name: The name to remove (not '\0'-terminated)
 * @param length: The length of the name
 * @return 0, -ENOENT (also for kinds the VFS cannot show, like symlinks), -EISDIR for a
 * directory, -EROFS for an indexed directory, -EIO for a file with no links, or -errno
 */
int ext2_unlink(vfs_node_t *dir, const char *name, size_t length)
{
    ext2_node_t *d = EXT2_NODE(dir);
    ext2_volume_t *volume = d->volume;
    vfs_node_t *child = NULL;
    int ret = 0;

    scoped_guard(mutex, &d->lock)
    {
        if (d->flags & EXT2_INDEX_FL)
            ret = -EROFS; // a hashed directory: its index would still list the name

        // The name's inode, as ext2_lookup finds it (a symlink is ENOENT: its target may live in i_block)
        ext2_lookup_context_t ctx = {
            .name = name,
            .length = length,
        };
        vfs_node_type_t type;
        if (ret == 0)
        {
            // The walk's 1 (found) becomes 0 (go on), its 0 (not found) ENOENT
            ret = ext2_dir_walk(d, ext2_lookup_visit, &ctx);
            if (ret == 0 || (ret == 1 && !ext2_entry_type(ctx.file_type, &type)))
                ret = -ENOENT;
            else if (ret == 1)
                ret = 0;
        }

        // The node before anything changes
        if (ret == 0)
            ret = ext2_node_get(volume, ctx.ino, dir, &child);
        if (ret == 0 && child->type == VFS_DIRECTORY)
            ret = -EISDIR; // rmdir's (W6e)
        if (ret == 0 && child->nlink == 0)
            ret = -EIO; // a name for a file with no links: the disk is corrupt

        if (ret == 0)
            ret = ext2_dir_remove(d, name, length);

        // One name less: on the disk, and in the node; at 0 the last put frees it
        if (ret == 0)
        {
            ext2_node_t *c = EXT2_NODE(child);
            scoped_guard(mutex, &c->lock)
            {
                uint16_t links = (uint16_t)--child->nlink;
                uint64_t byte = ext2_inode_byte(volume, c->ino);
                ret = ext2_disk_write(volume, byte + offsetof(ext2_inode_t, i_links_count), &links, sizeof(links));
                if (child->nlink == 0)
                    c->free_on_release = true;
            }
        }
    }

    // After the directory's lock: if nobody has the file open, this put frees it right here
    if (child != NULL)
        vfs_node_put(child);

    return ret;
}

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

void ext2_init(void)
{
    for (size_t i = 0; block_at(i) != NULL; i++)
    {
        block_device_t *disk = block_at(i);

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
