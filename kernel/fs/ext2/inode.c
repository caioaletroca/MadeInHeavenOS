#include "internal.h"
#include <mihos/errno.h>
#include <mm/kmalloc.h>
#include <string.h>

// ---- Inodes and nodes: where an inode lives, reading it into a node, writing it back; the ops tables ----

static const vfs_node_ops_t ext2_ops = {
    .lookup = ext2_lookup,
    .read = ext2_read,
    .write = ext2_write,
    .readdir = ext2_readdir,
    .truncate = ext2_truncate,
    .create = ext2_create,
    .unlink = ext2_unlink,
    .release = ext2_release,
};

static const vfs_node_ops_t ext2_read_only_ops = {
    .lookup = ext2_lookup,
    .read = ext2_read,
    .readdir = ext2_readdir,
    .release = ext2_release,
};

/**
 * @brief Where an inode is on the disk: its group's part of the inode table, then its slot
 *
 * @param volume: The ext2 filesystem
 * @param ino: The inode number (1-based, already checked)
 * @return The byte offset of the inode on the disk
 */
uint64_t ext2_inode_byte(const ext2_volume_t *volume, uint32_t ino)
{
    uint32_t group = (ino - 1) / volume->inodes_per_group;
    uint64_t slot = (ino - 1) % volume->inodes_per_group;
    return (uint64_t)volume->group_table[group].bg_inode_table * volume->block_size + slot * volume->inode_size;
}

/**
 * @brief Store an ext2 node's size and block map (i_size, i_size_high, i_block, i_blocks)
 * back into its inode on disk.
 *
 * @param n: The ext2 node to store
 * @return int: 0 on success, negative error code on failure
 */
int ext2_inode_store(ext2_node_t *n)
{
    ext2_volume_t *volume = n->volume;
    uint64_t byte = ext2_inode_byte(n->volume, n->ino);
    uint32_t size = (uint32_t)n->base.size;
    uint32_t size_high = (uint32_t)(n->base.size >> 32);

    int ret = ext2_disk_write(volume, byte + offsetof(ext2_inode_t, i_size), &size, sizeof(size));
    if (ret == 0)
        ret = ext2_disk_write(volume, byte + offsetof(ext2_inode_t, i_blocks), &n->blocks, sizeof(n->blocks));
    if (ret == 0)
        ret = ext2_disk_write(volume, byte + offsetof(ext2_inode_t, i_block), &n->block, sizeof(n->block));
    if (ret == 0 && n->base.type == VFS_FILE)
        ret = ext2_disk_write(volume, byte + offsetof(ext2_inode_t, i_size_high), &size_high, sizeof(size_high));

    return ret;
}

/**
 * @brief Get an inode from the ext2 filesystem
 *
 * @param volume: The ext2 filesystem structure
 * @param ino: The inode number to get
 * @param parent: The parent VFS node
 * @param out: Pointer to store the resulting VFS node
 * @return 0 on success, negative error code on failure
 */
int ext2_node_get(ext2_volume_t *volume, uint32_t ino, vfs_node_t *parent, vfs_node_t **out)
{
    if (ino == 0 || ino > volume->inodes_count)
        return -EIO;

    guard(mutex, &volume->lock);

    hlist_head_t *bucket = &volume->nodes[hash_64(ino, EXT2_NODE_HASH_BITS)];
    hlist_node_t *pos;
    hlist_for_each(pos, bucket)
    {
        ext2_node_t *cached = hlist_container(pos, ext2_node_t, cache_link);
        if (cached->ino != ino)
            continue;

        if (vfs_node_tryget(&cached->base))
        {
            *out = &cached->base;
            return 0;
        }

        // At 0 references: its release is on the way; take it out, read a fresh one
        hlist_remove(&cached->cache_link);
        break;
    }

    // Where the inode is (ext2_inode_byte): the ext2 block holding it, then its place in that block
    uint64_t byte = ext2_inode_byte(volume, ino);
    uint32_t block = (uint32_t)(byte / volume->block_size);

    block_cache_buf_t *lease;
    const uint8_t *data;
    int ret = ext2_block_get(volume, block, &lease, &data);
    if (ret < 0)
        return ret;

    ext2_inode_t inode;
    memcpy(&inode, data + byte % volume->block_size, sizeof(inode));
    block_cache_put(lease);

    vfs_node_type_t type;
    switch (inode.i_mode & EXT2_S_IFMT)
    {
    case EXT2_S_IFREG:
        type = VFS_FILE;
        break;
    case EXT2_S_IFDIR:
        type = VFS_DIRECTORY;
        break;
    default:
        return -ENOENT;
    }

    ext2_node_t *n = kzalloc(sizeof(*n));
    if (n == NULL)
        return -ENOMEM;

    vfs_node_init(&n->base, type, volume->read_only ? &ext2_read_only_ops : &ext2_ops);
    n->base.ino = ino;
    n->base.dev = volume->dev;
    n->base.mode = inode.i_mode & 07777;
    n->base.mtime = inode.i_mtime;
    n->base.nlink = inode.i_links_count;
    n->base.size = inode.i_size;
    if (type == VFS_FILE)
        n->base.size |= (uint64_t)inode.i_size_high << 32; // large_file: sizes past 4 GiB

    // ".." of the root is the root itself (the VFS steps out of mounts through mount_point)
    n->base.parent = parent != NULL ? parent : &n->base;
    if (parent != NULL)
        vfs_node_get(parent); // a child keeps its directory alive, so ".." stays valid

    n->volume = volume;
    n->ino = ino;
    n->blocks = inode.i_blocks;
    n->flags = inode.i_flags;
    memcpy(n->block, inode.i_block, sizeof(n->block));
    mutex_init(&n->lock);
    hlist_insert(bucket, &n->cache_link);

    *out = &n->base;
    return 0;
}

int ext2_inode_write(vfs_node_t *node)
{
    ext2_node_t *n = EXT2_NODE(node);
    guard(mutex, &n->lock);
    return ext2_inode_store(n);
}
