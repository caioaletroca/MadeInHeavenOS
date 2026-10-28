#include "internal.h"
#include <mihos/errno.h>
#include <string.h>

// ---- Directories: walking, adding and removing the records of a directory's blocks; readdir, lookup ----

/**
 * @brief Check if an ext2 directory record is bad
 *
 * @param volume The ext2 volume
 * @param data The block data containing the directory entry
 * @param offset The offset of the directory entry within the block
 * @return true if the record is bad, false otherwise
 */
static bool ext2_record_bad(const ext2_volume_t *volume, const uint8_t *data, uint32_t offset)
{
    if (volume->block_size - offset < sizeof(ext2_dirent_t))
        return true;

    const ext2_dirent_t *e = (const ext2_dirent_t *)(data + offset);
    return e->rec_len < sizeof(ext2_dirent_t) ||
           e->rec_len % 4 != 0 ||
           e->rec_len > volume->block_size - offset ||
           sizeof(ext2_dirent_t) + e->name_len > e->rec_len;
}

/**
 * @brief Convert an ext2 directory entry type to a VFS node type
 *
 * @param file_type: The ext2 directory entry type
 * @param type: The resulting VFS node type
 * @return true if the conversion was successful, false otherwise
 */
bool ext2_entry_type(uint8_t file_type, vfs_node_type_t *type)
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
 * @brief Walk through the entries of a directory
 *
 * The caller holds dir->lock: a create may split a record or add a block meanwhile.
 *
 * @param dir: The ext2 directory node
 * @param visit: The visit function to call for each directory entry
 * @param context: The context to pass to the visit function
 * @return 0 on success, negative error code on failure
 */
int ext2_dir_walk(ext2_node_t *dir, ext2_visit_t visit, void *context)
{
    ext2_volume_t *volume = dir->volume;
    uint64_t blocks = (dir->base.size + volume->block_size - 1) / volume->block_size;

    for (uint64_t i = 0; i < blocks; i++)
    {
        uint32_t block;
        int ret = ext2_file_block(volume, dir, i, NULL, false, &block);
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
            if (ext2_record_bad(volume, data, offset))
            {
                block_cache_put(lease);
                return -EIO;
            }

            const ext2_dirent_t *e = (const ext2_dirent_t *)(data + offset);
            rec_len = e->rec_len;
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
 * @brief Link a name to an inode in a directory: in the first record with room after what it
 * uses (split off it), or in a new block at the end holding one record that covers all of it
 *
 * The caller holds dir->lock and has checked the name is not there.
 *
 * @param dir: The ext2 directory node
 * @param name: The name of the new entry
 * @param length: The length of the name
 * @param ino: The inode number of the new entry
 * @param file_type: The file type of the new entry
 * @return 0 on success, -EIO for a corrupt record, or the error of a new block
 */
int ext2_dir_add(ext2_node_t *dir, const char *name, size_t length, uint32_t ino, uint8_t file_type)
{
    ext2_volume_t *volume = dir->volume;
    uint32_t need = EXT2_DIRENT_SIZE(length);
    uint64_t blocks = dir->base.size / volume->block_size;

    for (uint64_t i = 0; i < blocks; i++)
    {
        uint32_t block;
        int ret = ext2_file_block(volume, dir, i, NULL, false, &block);
        if (ret < 0)
            return ret;
        if (block == 0)
            continue;

        block_cache_buf_t *lease;
        const uint8_t *data;
        ret = ext2_block_get(volume, block, &lease, &data);
        if (ret < 0)
            return ret;

        uint32_t rec_len;
        for (uint32_t offset = 0; offset < volume->block_size; offset += rec_len)
        {
            if (ext2_record_bad(volume, data, offset))
            {
                block_cache_put(lease);
                return -EIO;
            }

            const ext2_dirent_t *e = (const ext2_dirent_t *)(data + offset);
            rec_len = e->rec_len;

            uint32_t used = e->inode != 0 ? EXT2_DIRENT_SIZE(e->name_len) : 0; // a deleted record uses nothing
            if (rec_len - used < need)
                continue;

            // The writable address of this record (data is const: the same trick as ext2_block_write)
            uint8_t *record = lease->data + (data - lease->data) + offset;
            ext2_dirent_t *added = (ext2_dirent_t *)(record + used);

            block_cache_begin_write(lease);
            if (used != 0)
                ((ext2_dirent_t *)record)->rec_len = used; // the old record keeps only what it uses
            added->inode = ino;
            added->rec_len = rec_len - used; // the rest, so the chain still ends at the block's end
            added->name_len = (uint8_t)length;
            added->file_type = file_type;
            memcpy(added->name, name, length);
            block_cache_end_write(lease);

            block_cache_put(lease);
            return 0;
        }

        block_cache_put(lease);
    }

    // No record has room: a new block at the end, holding one record that covers all of it
    uint32_t block;
    int ret = ext2_file_block(volume, dir, blocks, NULL, true, &block); // allocated zeroed, counted in dir->blocks
    if (ret < 0)
        return ret;

    uint8_t record[sizeof(ext2_dirent_t) + VFS_NAME_MAX] = {0};
    ext2_dirent_t *added = (ext2_dirent_t *)record;

    added->inode = ino;
    added->rec_len = (uint16_t)volume->block_size;
    added->name_len = (uint8_t)length;
    added->file_type = file_type;
    memcpy(added->name, name, length);

    ret = ext2_block_write(volume, block, 0, record, need);
    if (ret < 0)
        return ret;

    dir->base.size += volume->block_size;
    return ext2_inode_store(dir);
}

/**
 * @brief Remove the record called name[0..length) from a directory: the record before it in its
 * block swallows it (its rec_len grows by the removed one's), or, first in its block, it is
 * marked unused (inode 0). The caller holds dir->lock.
 *
 * @param dir: The directory node
 * @param name: The name of the entry to remove (not '\0'-terminated)
 * @param length: The length of the name
 * @return 0, -ENOENT if not there, -EIO for a corrupt record, or the disk's error
 */
int ext2_dir_remove(ext2_node_t *dir, const char *name, size_t length)
{
    ext2_volume_t *volume = dir->volume;
    uint64_t blocks = dir->base.size / volume->block_size;

    for (uint64_t i = 0; i < blocks; i++)
    {
        uint32_t block;
        int ret = ext2_file_block(volume, dir, i, NULL, false, &block);
        if (ret < 0)
            return ret;
        if (block == 0)
            continue;

        block_cache_buf_t *lease;
        const uint8_t *data;
        ret = ext2_block_get(volume, block, &lease, &data);
        if (ret < 0)
            return ret;

        // `previous` follows one record behind, deleted ones included (growing one of those is fine)
        uint32_t rec_len = 0;
        uint32_t previous = UINT32_MAX; // none yet in this block
        for (uint32_t offset = 0; offset < volume->block_size; previous = offset, offset += rec_len)
        {
            if (ext2_record_bad(volume, data, offset))
            {
                block_cache_put(lease);
                return -EIO;
            }

            const ext2_dirent_t *e = (const ext2_dirent_t *)(data + offset);
            rec_len = e->rec_len;
            if (e->inode == 0 || e->name_len != length || memcmp(e->name, name, length) != 0)
                continue;

            uint8_t *start = lease->data + (data - lease->data); // the block's writable first byte
            block_cache_begin_write(lease);
            if (previous == UINT32_MAX)
                ((ext2_dirent_t *)(start + offset))->inode = 0;
            else
                ((ext2_dirent_t *)(start + previous))->rec_len += rec_len;

            block_cache_end_write(lease);

            block_cache_put(lease);
            return 0;
        }

        block_cache_put(lease);
    }

    return -ENOENT;
}

/**
 * @brief Visit function for checking if a directory is empty
 *
 * @param e: The ext2 directory entry
 * @param context: Unused context
 * @return int: 0 for '.' or '..', 1 for other entries
 */
static int ext2_empty_visit(const ext2_dirent_t *e, void *context)
{
    (void)context;
    bool dot = e->name_len == 1 && e->name[0] == '.';
    bool dotdot = e->name_len == 2 && e->name[0] == '.' && e->name[1] == '.';
    return dot || dotdot ? 0 : 1;
}

/**
 * @brief Check if a directory is empty
 *
 * @param dir: The ext2 directory node
 * @return int: 1 if empty, 0 if not, negative error code on failure
 */
int ext2_dir_empty(ext2_node_t *dir)
{
    int ret = ext2_dir_walk(dir, ext2_empty_visit, NULL);
    return ret < 0 ? ret : ret == 0;
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
int ext2_readdir(vfs_node_t *dir, uint64_t index, vfs_dirent_t *entry)
{
    guard(mutex, &EXT2_NODE(dir)->lock);

    ext2_readdir_context_t ctx = {
        .index = index,
        .count = 0,
        .entry = entry};
    return ext2_dir_walk(EXT2_NODE(dir), ext2_readdir_visit, &ctx);
}

/**
 * @brief Visit function for looking up a directory entry by name
 *
 * @param e: The ext2 directory entry
 * @param context: The lookup context
 * @return int: 1 if the entry matches the name, 0 otherwise
 */
int ext2_lookup_visit(const ext2_dirent_t *e, void *context)
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
int ext2_lookup(vfs_node_t *dir, const char *name, size_t length, vfs_node_t **child)
{
    ext2_lookup_context_t ctx = {
        .name = name,
        .length = length,
    };
    int ret;
    scoped_guard(mutex, &EXT2_NODE(dir)->lock)
    {
        ret = ext2_dir_walk(EXT2_NODE(dir), ext2_lookup_visit, &ctx);
    }
    if (ret < 0)
        return ret;

    vfs_node_type_t type;
    if (ret == 0 || !ext2_entry_type(ctx.file_type, &type))
        return -ENOENT; // not there, or a kind the VFS cannot show yet

    // After the walk, with no lease held: ext2_node_get takes volume->lock and may read the disk
    return ext2_node_get(EXT2_NODE(dir)->volume, ctx.ino, dir, child);
}
