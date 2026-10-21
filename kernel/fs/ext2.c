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

typedef struct ext2_node
{
    vfs_node_t node; // what the VFS sees (the "base class")
    ext2_fs_t *fs;
    uint32_t ino;
    uint32_t block[EXT2_N_BLOCKS]; // copy of the inode's i_block
    hlist_node_t hash;             // in fs->nodes, under fs->lock
} ext2_node_t;

#define EXT2_NODE(n) list_container(n, ext2_node_t, node)

static int ext2_lookup(vfs_node_t *dir, const char *name, size_t length, vfs_node_t **child);
static long ext2_read(vfs_node_t *file, void *buffer, size_t count, uint64_t offset);
static int ext2_readdir(vfs_node_t *dir, uint64_t index, vfs_dirent_t *entry);
static void ext2_release(vfs_node_t *node);

static const vfs_node_ops_t ext2_ops = {
    .lookup = ext2_lookup,
    .read = ext2_read,
    .readdir = ext2_readdir,
    .release = ext2_release,
};

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

/**
 * @brief Get an inode from the ext2 filesystem
 *
 * @param fs: The ext2 filesystem structure
 * @param ino: The inode number to get
 * @param parent: The parent VFS node
 * @param out: Pointer to store the resulting VFS node
 * @return 0 on success, negative error code on failure
 */
static int ext2_iget(ext2_fs_t *fs, uint32_t ino, vfs_node_t *parent, vfs_node_t **out)
{
    if (ino == 0 || ino > fs->inodes_count)
        return -EIO;

    guard(mutex, &fs->lock);

    hlist_head_t *bucket = &fs->nodes[hash_64(ino, EXT2_NODE_HASH_BITS)];
    hlist_node_t *pos;
    hlist_for_each(pos, bucket)
    {
        ext2_node_t *cached = hlist_container(pos, ext2_node_t, hash);
        if (cached->ino != ino)
            continue;

        if (vfs_node_tryget(&cached->node))
        {
            *out = &cached->node;
            return 0;
        }

        // At 0 references: its release is on the way; take it out, read a fresh one
        hlist_remove(&cached->hash);
        break;
    }

    // Where the inode is: its group's part of the inode table, then the slot in it
    uint32_t group = (ino - 1) / fs->inodes_per_group;
    uint64_t offset = (uint64_t)((ino - 1) % fs->inodes_per_group) * fs->inode_size;
    uint32_t block = fs->group_table[group].bg_inode_table + (uint32_t)(offset / fs->block_size);

    bcache_buf_t *lease;
    const uint8_t *data;
    int ret = ext2_block_get(fs, block, &lease, &data);
    if (ret < 0)
        return ret;

    ext2_inode_t inode;
    memcpy(&inode, data + offset % fs->block_size, sizeof(inode));
    bcache_put(lease);

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

    vfs_node_init(&n->node, type, &ext2_ops);
    n->node.ino = ino;
    n->node.dev = fs->dev;
    n->node.mode = inode.i_mode & 07777;
    n->node.mtime = inode.i_mtime;
    n->node.nlink = inode.i_links_count;
    n->node.size = inode.i_size;
    if (type == VFS_FILE)
        n->node.size |= (uint64_t)inode.i_size_high << 32; // large_file: sizes past 4 GiB

    // ".." of the root is the root itself (the VFS steps out of mounts through covers)
    n->node.parent = parent != NULL ? parent : &n->node;
    if (parent != NULL)
        vfs_node_get(parent); // a child keeps its directory alive, so ".." stays valid

    n->fs = fs;
    n->ino = ino;
    memcpy(n->block, inode.i_block, sizeof(n->block));
    hlist_insert(bucket, &n->hash);

    *out = &n->node;
    return 0;
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

    vfs_node_t *node;
    ret = ext2_iget(fs, EXT2_ROOT_INO, NULL, &node);
    if (ret == 0 && node->type != VFS_DIRECTORY)
    {
        vfs_node_put(node);
        ret = -EIO;
    }
    if (ret < 0)
    {
        ext2_fs_free(fs);
        return ret;
    }

    *root = node;
    return 0;
}

/**
 * @brief Map a logical block number to a physical block number in an ext2 file
 *
 * @param fs: The ext2 filesystem structure
 * @param n: The ext2 node
 * @param logical: The logical block number
 * @param out: The resulting physical block number
 * @return 0 on success, negative error code on failure
 */
static int ext2_bmap(ext2_fs_t *fs, const ext2_node_t *n, uint64_t logical, uint32_t *out)
{
    if (logical < EXT2_NDIR_BLOCKS)
    {
        *out = n->block[logical];
        return 0;
    }
    logical -= EXT2_NDIR_BLOCKS;

    uint64_t per_block = fs->block_size / sizeof(uint32_t); // number of block pointers per indirect block
    uint64_t span = per_block;                              // number of blocks spanned by a single indirect block

    for (int levels = 1; levels <= 3; levels++, span *= per_block)
    {
        if (logical >= span)
        {
            logical -= span;
            continue;
        }

        // Walk down: at each level, which entry, and what is left below it
        uint32_t block = n->block[EXT2_NDIR_BLOCKS + levels - 1];
        for (uint64_t below = span / per_block; block != 0; below /= per_block)
        {
            bcache_buf_t *lease;
            const uint8_t *data;
            int ret = ext2_block_get(fs, block, &lease, &data);
            if (ret < 0)
                return ret;

            block = ((const uint32_t *)data)[logical / below];
            bcache_put(lease);

            logical %= below;
            if (below == 1)
                break; // that was the last level: block is the data block
        }

        *out = block;
        return 0;
    }

    return -EIO; // logical block number out of range
}

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
    ext2_fs_t *fs = dir->fs;
    uint64_t blocks = (dir->node.size + fs->block_size - 1) / fs->block_size;

    for (uint64_t i = 0; i < blocks; i++)
    {
        uint32_t block;
        int ret = ext2_bmap(fs, dir, i, &block);
        if (ret < 0)
            return ret;
        if (block == 0)
            continue;

        bcache_buf_t *lease;
        const uint8_t *data;
        ret = ext2_block_get(fs, block, &lease, &data);
        if (ret < 0)
            return ret;

        // Records chain through rec_len and end exactly at the end of the block
        uint32_t rec_len;
        for (uint32_t offset = 0; offset < fs->block_size; offset += rec_len)
        {
            const ext2_dirent_t *e = (const ext2_dirent_t *)(data + offset);

            // Check before trusting the disk: room for a header, then a rec_len that
            // moves forward (0 would loop forever), stays in the block and holds the name
            bool corrupt = fs->block_size - offset < sizeof(ext2_dirent_t);
            if (!corrupt)
            {
                rec_len = e->rec_len;
                corrupt = rec_len < sizeof(ext2_dirent_t) || rec_len % 4 != 0 ||
                          rec_len > fs->block_size - offset ||
                          sizeof(ext2_dirent_t) + e->name_len > rec_len;
            }
            if (corrupt)
            {
                bcache_put(lease);
                return -EIO;
            }

            if (e->inode == 0)
                continue; // a deleted entry (or an empty block's placeholder)

            ret = visit(e, context);
            if (ret != 0)
            {
                bcache_put(lease);
                return ret;
            }
        }

        bcache_put(lease);
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

    // After the walk, with no lease held: iget takes fs->lock and may read the disk
    return ext2_iget(EXT2_NODE(dir)->fs, ctx.ino, dir, child);
}

// File reads come in the next step
static long ext2_read(vfs_node_t *file, void *buffer, size_t count, uint64_t offset)
{
    (void)file;
    (void)buffer;
    (void)count;
    (void)offset;
    return -ENOSYS;
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
 * @brief Release an ext2 node
 *
 * @param node: The VFS node to release
 */
static void ext2_release(vfs_node_t *node)
{
    ext2_node_t *n = EXT2_NODE(node);

    scoped_guard(mutex, &n->fs->lock)
    {
        if (hlist_linked(&n->hash))
            hlist_remove(&n->hash);
    }

    vfs_node_t *parent = node->parent != node ? node->parent : NULL;
    kfree(n);

    if (parent != NULL)
        vfs_node_put(parent);
}
