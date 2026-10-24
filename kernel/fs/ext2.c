#include <fs/ext2.h>
#include "ext2_disk.h"
#include <driver/block.h>
#include <driver/block_cache.h>
#include <sched/sync.h>
#include <sys/hashtable.h>
#include <mihos/errno.h>
#include <mm/kmalloc.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include <kprintf.h>
#include <util.h>

#define EXT2_NODE_HASH_BITS 6 // 64 buckets for cached nodes

typedef struct ext2_volume
{
    block_device_t *disk;
    uint64_t dev;              // st_dev (vfs_dev_alloc)
    uint32_t block_size;       // 1024, 2048 or 4096
    uint32_t blocks_count;     // valid block numbers: 1 .. blocks_count - 1
    uint32_t first_data_block; // s_first_data_block: 1 with 1 KiB blocks, else 0; bit i of a group's bitmap is block first_data_block + group * blocks_per_group + i
    uint32_t blocks_per_group;
    uint32_t inodes_count;
    uint32_t inodes_per_group;
    uint32_t inode_size; // bytes per inode in the table (128 or more)
    uint32_t groups;
    bool read_only;            // an unknown ro_compat feature: writing could damage the disk
    ext2_group_t *group_table; // kmalloc'd copy of the group descriptors
    mutex_t lock;              // the node cache: nodes[] and every node's hash link
    hlist_head_t nodes[HASH_BUCKETS(EXT2_NODE_HASH_BITS)];
} ext2_volume_t;

typedef struct ext2_node
{
    vfs_node_t base; // what the VFS sees (the "base class")
    ext2_volume_t *volume;
    uint32_t ino;
    uint32_t block[EXT2_BLOCK_POINTERS]; // copy of the inode's i_block
    uint32_t blocks;                     // i_blocks: 512-byte units the file owns, indirect tables included
    mutex_t lock;                        // block[], blocks and base.size: held by reads and writes of the file
    hlist_node_t cache_link;             // in volume->nodes, under volume->lock
} ext2_node_t;

#define EXT2_NODE(n) list_container(n, ext2_node_t, base)

static int ext2_lookup(vfs_node_t *dir, const char *name, size_t length, vfs_node_t **child);
static long ext2_read(vfs_node_t *file, void *buffer, size_t count, uint64_t offset);
static long ext2_write(vfs_node_t *file, const void *buffer, size_t count, uint64_t offset);
static int ext2_readdir(vfs_node_t *dir, uint64_t index, vfs_dirent_t *entry);
static void ext2_release(vfs_node_t *node);

static const vfs_node_ops_t ext2_ops = {
    .lookup = ext2_lookup,
    .read = ext2_read,
    .write = ext2_write,
    .readdir = ext2_readdir,
    .release = ext2_release,
};

static const vfs_node_ops_t ext2_read_only_ops = {
    .lookup = ext2_lookup,
    .read = ext2_read,
    .readdir = ext2_readdir,
    .release = ext2_release,
};

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
static int ext2_block_get(ext2_volume_t *volume, uint32_t block, block_cache_buf_t **lease, const uint8_t **data)
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
static int ext2_block_write(ext2_volume_t *volume, uint32_t block, size_t within, const uint8_t *data, size_t count)
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
static int ext2_disk_write(ext2_volume_t *volume, uint64_t byte, const void *data, size_t count)
{
    long ret = block_write_bytes(volume->disk, data, count, byte);
    if (ret < 0)
        return ret;
    if (ret != (long)count)
        return -EIO;
    return 0;
}

// ---- Inodes and nodes: where an inode lives, reading it into a node, writing it back ----

/**
 * @brief Where an inode is on the disk: its group's part of the inode table, then its slot
 *
 * @param volume: The ext2 filesystem
 * @param ino: The inode number (1-based, already checked)
 * @return The byte offset of the inode on the disk
 */
static uint64_t ext2_inode_byte(const ext2_volume_t *volume, uint32_t ino)
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
static int ext2_inode_store(ext2_node_t *n)
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
static int ext2_node_get(ext2_volume_t *volume, uint32_t ino, vfs_node_t *parent, vfs_node_t **out)
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

/**
 * @brief Release an ext2 node
 *
 * @param node: The VFS node to release
 */
static void ext2_release(vfs_node_t *node)
{
    ext2_node_t *n = EXT2_NODE(node);

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

// ---- File blocks: a file's logical block number to its block on the disk, through the tables ----

/**
 * @brief Map a logical block number to a physical block number in an ext2 file
 *
 * @param volume: The ext2 filesystem structure
 * @param n: The ext2 node
 * @param logical: The logical block number
 * @param out: The resulting physical block number
 * @return 0 on success, negative error code on failure
 */
static int ext2_file_block(ext2_volume_t *volume, const ext2_node_t *n, uint64_t logical, uint32_t *out)
{
    if (logical < EXT2_DIRECT_BLOCKS)
    {
        *out = n->block[logical];
        return 0;
    }
    logical -= EXT2_DIRECT_BLOCKS;

    uint64_t per_block = volume->block_size / sizeof(uint32_t); // number of block pointers per indirect block
    uint64_t span = per_block;                                  // number of blocks spanned by a single indirect block

    for (int levels = 1; levels <= 3; levels++, span *= per_block)
    {
        if (logical >= span)
        {
            logical -= span;
            continue;
        }

        // Walk down: at each level, which entry, and what is left below it
        uint32_t block = n->block[EXT2_DIRECT_BLOCKS + levels - 1];
        for (uint64_t below = span / per_block; block != 0; below /= per_block)
        {
            block_cache_buf_t *lease;
            const uint8_t *data;
            int ret = ext2_block_get(volume, block, &lease, &data);
            if (ret < 0)
                return ret;

            block = ((const uint32_t *)data)[logical / below];
            block_cache_put(lease);

            logical %= below;
            if (below == 1)
                break; // that was the last level: block is the data block
        }

        *out = block;
        return 0;
    }

    return -EIO; // logical block number out of range
}

// ---- Directories: walking the records of a directory's blocks ----

/**
 * @brief Convert an ext2 directory entry type to a VFS node type
 *
 * @param file_type: The ext2 directory entry type
 * @param type: The resulting VFS node type
 * @return true if the conversion was successful, false otherwise
 */
static bool ext2_entry_type(uint8_t file_type, vfs_node_type_t *type)
{
    switch (file_type)
    {
    case EXT2_FT_REG_FILE:
        *type = VFS_FILE;
        return true;
    case EXT2_FT_DIR:
        *type = VFS_DIRECTORY;
        return true;
    default:
        return false;
    }
}

/**
 * Called for each used entry of a directory, while its block is leased:
 * `entry` points into the cache buffer and is only valid during the call.
 *
 * @return 0 to go on, 1 to stop the walk, or -errno to fail it.
 */
typedef int (*ext2_visit_t)(const ext2_dirent_t *entry, void *context);

/**
 * @brief Walk through the entries of a directory
 *
 * @param dir: The ext2 directory node
 * @param visit: The visit function to call for each directory entry
 * @param context: The context to pass to the visit function
 * @return 0 on success, negative error code on failure
 */
static int ext2_dir_walk(ext2_node_t *dir, ext2_visit_t visit, void *context)
{
    ext2_volume_t *volume = dir->volume;
    uint64_t blocks = (dir->base.size + volume->block_size - 1) / volume->block_size;

    for (uint64_t i = 0; i < blocks; i++)
    {
        uint32_t block;
        int ret = ext2_file_block(volume, dir, i, &block);
        if (ret < 0)
            return ret;
        if (block == 0)
            continue;

        block_cache_buf_t *lease;
        const uint8_t *data;
        ret = ext2_block_get(volume, block, &lease, &data);
        if (ret < 0)
            return ret;

        // Records chain through rec_len and end exactly at the end of the block
        uint32_t rec_len;
        for (uint32_t offset = 0; offset < volume->block_size; offset += rec_len)
        {
            const ext2_dirent_t *e = (const ext2_dirent_t *)(data + offset);

            // Check before trusting the disk: room for a header, then a rec_len that
            // moves forward (0 would loop forever), stays in the block and holds the name
            bool corrupt = volume->block_size - offset < sizeof(ext2_dirent_t);
            if (!corrupt)
            {
                rec_len = e->rec_len;
                corrupt = rec_len < sizeof(ext2_dirent_t) || rec_len % 4 != 0 ||
                          rec_len > volume->block_size - offset ||
                          sizeof(ext2_dirent_t) + e->name_len > rec_len;
            }
            if (corrupt)
            {
                block_cache_put(lease);
                return -EIO;
            }

            if (e->inode == 0)
                continue; // a deleted entry (or an empty block's placeholder)

            ret = visit(e, context);
            if (ret != 0)
            {
                block_cache_put(lease);
                return ret;
            }
        }

        block_cache_put(lease);
    }

    return 0;
}

/**
 * @brief Context for reading directory entries
 */
typedef struct ext2_readdir_context
{
    uint64_t index;
    uint64_t count;
    vfs_dirent_t *entry;
} ext2_readdir_context_t;

/**
 * @brief Visit function for reading directory entries
 *
 * @param e: The ext2 directory entry
 * @param context: The readdir context
 * @return int: 1 if the entry matches the index, 0 otherwise
 */
static int ext2_readdir_visit(const ext2_dirent_t *e, void *context)
{
    ext2_readdir_context_t *ctx = context;

    vfs_node_type_t type;
    if (!ext2_entry_type(e->file_type, &type))
        return 0;

    if (ctx->count++ != ctx->index)
        return 0;

    ctx->entry->ino = e->inode;
    ctx->entry->type = type;
    ctx->entry->length = e->name_len;
    memcpy(ctx->entry->name, e->name, e->name_len);
    ctx->entry->name[e->name_len] = '\0';

    return 1;
}

/**
 * @brief Read a directory entry by index
 *
 * @param dir: The directory VFS node
 * @param index: The index of the entry to read
 * @param entry: Pointer to store the resulting directory entry
 * @return int: 0 on success, negative error code on failure
 */
static int ext2_readdir(vfs_node_t *dir, uint64_t index, vfs_dirent_t *entry)
{
    ext2_readdir_context_t ctx = {
        .index = index,
        .count = 0,
        .entry = entry};
    return ext2_dir_walk(EXT2_NODE(dir), ext2_readdir_visit, &ctx);
}

/**
 * @brief Context for looking up a directory entry by name
 */
typedef struct ext2_lookup_context
{
    const char *name;
    size_t length;
    uint32_t ino;
    uint8_t file_type;
} ext2_lookup_context_t;

/**
 * @brief Visit function for looking up a directory entry by name
 *
 * @param e: The ext2 directory entry
 * @param context: The lookup context
 * @return int: 1 if the entry matches the name, 0 otherwise
 */
static int ext2_lookup_visit(const ext2_dirent_t *e, void *context)
{
    ext2_lookup_context_t *ctx = context;

    if (e->name_len != ctx->length)
        return 0;

    if (memcmp(e->name, ctx->name, ctx->length) != 0)
        return 0;

    ctx->ino = e->inode;
    ctx->file_type = e->file_type;

    return 1;
}

/**
 * @brief Look up a directory entry by name
 *
 * @param dir: The directory VFS node
 * @param name: The name of the entry to look up
 * @param length: The length of the name
 * @param child: Pointer to store the resulting child VFS node
 * @return int: 0 on success, negative error code on failure
 */
static int ext2_lookup(vfs_node_t *dir, const char *name, size_t length, vfs_node_t **child)
{
    ext2_lookup_context_t ctx = {
        .name = name,
        .length = length,
    };
    int ret = ext2_dir_walk(EXT2_NODE(dir), ext2_lookup_visit, &ctx);
    if (ret < 0)
        return ret;

    vfs_node_type_t type;
    if (ret == 0 || !ext2_entry_type(ctx.file_type, &type))
        return -ENOENT; // not there, or a kind the VFS cannot show yet

    // After the walk, with no lease held: ext2_node_get takes volume->lock and may read the disk
    return ext2_node_get(EXT2_NODE(dir)->volume, ctx.ino, dir, child);
}

// ---- File data ----

/**
 * @brief Read data from an ext2 file
 *
 * @param file: The VFS node representing the file
 * @param buffer: The buffer to read data into
 * @param count: The number of bytes to read
 * @param offset: The offset within the file to start reading from
 * @return long: The number of bytes read, or a negative error code on failure
 */
static long ext2_read(vfs_node_t *file, void *buffer, size_t count, uint64_t offset)
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
        int ret = ext2_file_block(volume, n, logical, &block);
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
static long ext2_write(vfs_node_t *file, const void *buffer, size_t count, uint64_t offset)
{
    ext2_node_t *n = EXT2_NODE(file);
    ext2_volume_t *volume = n->volume;

    guard(mutex, &n->lock);

    if (count == 0)
        return 0;
    if (offset >= file->size)
        return -ENOSPC; // growing the file needs new blocks (W5)

    count = MIN(count, file->size - offset);

    const uint8_t *in = buffer;
    size_t done = 0;
    while (done < count)
    {
        uint64_t position = offset + done;
        uint64_t logical = position / volume->block_size;
        size_t within = position % volume->block_size;
        size_t chunk = MIN(volume->block_size - within, count - done);

        uint32_t block;
        int ret = ext2_file_block(volume, n, logical, &block);
        if (ret < 0)
            return ret;

        if (block == 0)
            break; // a hole: writing it needs a new block (W5)

        ret = ext2_block_write(volume, block, within, in + done, chunk);
        if (ret < 0)
            return ret;

        done += chunk;
    }

    return done > 0 ? (long)done : -ENOSPC;
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
    volume->groups = groups;
    volume->read_only = (superblock->s_feature_ro_compat & ~EXT2_FEATURE_RO_COMPAT_KNOWN) != 0 ||
                        disk->ops->write == NULL;

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

    mutex_init(&volume->lock);
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
