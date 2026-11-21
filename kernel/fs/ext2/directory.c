#include "internal.h"
#include <mihos/errno.h>
#include <string.h>

// ---- Directories: a cursor over a directory's records; adding, removing and changing them; readdir, lookup ----

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
    case EXT2_FT_SYMLINK:
        *type = VFS_SYMLINK;
        return true;
    default:
        return false;
    }
}

/**
 * @brief The directory entry type for a VFS node type
 *
 * @param type: The VFS node type (a file, directory or symlink)
 * @return Its EXT2_FT_* value
 */
uint8_t ext2_file_type(vfs_node_type_t type)
{
    switch (type)
    {
    case VFS_DIRECTORY:
        return EXT2_FT_DIR;
    case VFS_SYMLINK:
        return EXT2_FT_SYMLINK;
    default:
        return EXT2_FT_REG_FILE;
    }
}

/**
 * @brief Step to the next record of the directory, unused ones included
 *
 * Walks the blocks in order (holes have no records) and the records of each
 * block through rec_len. A block's lease is held while the cursor points into it.
 *
 * @param c: The cursor (ext2_dir_cursor(dir) before the first step)
 * @return 1 at a record (c->entry), 0 past the last, -EIO for a corrupt record, or -errno
 */
int ext2_dir_next_record(ext2_dir_cursor_t *c)
{
    ext2_volume_t *volume = c->dir->volume;
    uint64_t blocks = c->dir->base.size / volume->block_size;

    // Step past the current record; the first step has none
    if (c->entry != NULL)
    {
        c->previous = c->offset;
        c->offset += c->entry->rec_len;
    }

    for (;;)
    {
        // Still inside a block: the record here
        if (c->lease != NULL && c->offset < volume->block_size)
        {
            if (ext2_record_bad(volume, c->data, c->offset))
                return -EIO;
            c->entry = (const ext2_dirent_t *)(c->data + c->offset);
            return 1;
        }

        // Past this block's end (records end exactly there): on to the next
        if (c->lease != NULL)
        {
            block_cache_put(c->lease);
            c->lease = NULL;
            c->block++;
        }
        if (c->block >= blocks)
        {
            c->entry = NULL;
            return 0;
        }

        uint32_t block;
        return_on_error(ext2_file_block(volume, c->dir, c->block, NULL, false, &block));
        if (block == 0)
        {
            c->block++; // a hole: no records there
            continue;
        }

        block_cache_buf_t *lease;
        const uint8_t *data;
        return_on_error(ext2_block_get(volume, block, &lease, &data));
        c->lease = lease;
        c->data = data;
        c->offset = 0;
        c->previous = UINT32_MAX;
    }
}

/**
 * @brief Step to the next used record (deleted ones, inode 0, are skipped)
 *
 * @param c: The cursor
 * @return 1 at a record (c->entry), 0 past the last, or -errno
 */
int ext2_dir_next(ext2_dir_cursor_t *c)
{
    int ret = 0;
    while ((ret = ext2_dir_next_record(c)) == 1 && c->entry->inode == 0)
        ;
    return ret;
}

/**
 * @brief Step to the next used record called name[0..length)
 *
 * @param c: The cursor
 * @param name: The name (not '\0'-terminated)
 * @param length: The length of the name
 * @return 1 at the record (c->entry), 0 if not there, or -errno
 */
int ext2_dir_find(ext2_dir_cursor_t *c, const char *name, size_t length)
{
    int ret = 0;
    while ((ret = ext2_dir_next(c)) == 1)
        if (c->entry->name_len == length && memcmp(c->entry->name, name, length) == 0)
            return 1;
    return ret;
}

/**
 * @brief Give back the lease a cursor holds (for release_with)
 *
 * @param c: The cursor
 */
void ext2_dir_cursor_close(ext2_dir_cursor_t *c)
{
    if (c->lease != NULL)
        block_cache_put(c->lease);
}

/**
 * @brief A record of the cursor's current block, writable: change it only between
 * block_cache_begin_write(c->lease) and block_cache_end_write(c->lease)
 *
 * c->data is const (the cache's read view); this is the same byte in the lease's
 * own buffer, the trick ext2_block_write uses.
 *
 * @param c: The cursor, at a record
 * @param offset: The record's offset in the block (c->offset, or c->previous)
 * @return The record
 */
ext2_dirent_t *ext2_dir_writable(ext2_dir_cursor_t *c, uint32_t offset)
{
    return (ext2_dirent_t *)(c->lease->data + (c->data - c->lease->data) + offset);
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

    ext2_dir_cursor_t c release_with(ext2_dir_cursor_close) = ext2_dir_cursor(dir);
    while (return_on_error(ext2_dir_next_record(&c)))
    {
        uint32_t rec_len = c.entry->rec_len;
        uint32_t used = c.entry->inode != 0 ? EXT2_DIRENT_SIZE(c.entry->name_len) : 0; // a deleted record uses nothing
        if (rec_len - used < need)
            continue;

        ext2_dirent_t *record = ext2_dir_writable(&c, c.offset);
        ext2_dirent_t *added = (ext2_dirent_t *)((uint8_t *)record + used);

        block_cache_begin_write(c.lease);
        if (used != 0)
            record->rec_len = used; // the old record keeps only what it uses
        added->inode = ino;
        added->rec_len = rec_len - used; // the rest, so the chain still ends at the block's end
        added->name_len = (uint8_t)length;
        added->file_type = file_type;
        memcpy(added->name, name, length);
        block_cache_end_write(c.lease);
        return 0;
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
    ext2_dir_cursor_t c release_with(ext2_dir_cursor_close) = ext2_dir_cursor(dir);
    if (!return_on_error(ext2_dir_find(&c, name, length)))
        return -ENOENT;

    // c.previous counts deleted records too (growing one of those is fine)
    block_cache_begin_write(c.lease);
    if (c.previous == UINT32_MAX)
        ext2_dir_writable(&c, c.offset)->inode = 0;
    else
        ext2_dir_writable(&c, c.previous)->rec_len += c.entry->rec_len;
    block_cache_end_write(c.lease);
    return 0;
}

/**
 * @brief Point an existing record at another inode, in place: the name, its place in the
 * block and its rec_len stay (a moved directory's ".." must stay its second record)
 *
 * The caller holds dir->lock.
 *
 * @param dir: The directory node
 * @param name: The record's name (not '\0'-terminated)
 * @param length: The length of the name
 * @param ino: The inode the record names from now on
 * @param file_type: That inode's EXT2_FT_* type
 * @return 0, -ENOENT if not there, -EIO for a corrupt record, or the disk's error
 */
int ext2_dir_set(ext2_node_t *dir, const char *name, size_t length, uint32_t ino, uint8_t file_type)
{
    ext2_dir_cursor_t c release_with(ext2_dir_cursor_close) = ext2_dir_cursor(dir);
    if (!return_on_error(ext2_dir_find(&c, name, length)))
        return -ENOENT;

    ext2_dirent_t *record = ext2_dir_writable(&c, c.offset);
    block_cache_begin_write(c.lease);
    record->inode = ino;
    record->file_type = file_type;
    block_cache_end_write(c.lease);
    return 0;
}

/**
 * @brief Whether a record is "." or ".."
 *
 * @param e: The record
 * @return true for "." and ".."
 */
static bool ext2_entry_is_dots(const ext2_dirent_t *e)
{
    return (e->name_len == 1 && e->name[0] == '.') ||
           (e->name_len == 2 && e->name[0] == '.' && e->name[1] == '.');
}

/**
 * @brief Check if a directory is empty: nothing but "." and ".."
 *
 * @param dir: The ext2 directory node; the caller holds its lock
 * @return 1 if empty, 0 if not, negative error code on failure
 */
int ext2_dir_empty(ext2_node_t *dir)
{
    ext2_dir_cursor_t c release_with(ext2_dir_cursor_close) = ext2_dir_cursor(dir);
    while (return_on_error(ext2_dir_next(&c)))
        if (!ext2_entry_is_dots(c.entry))
            return 0;
    return 1;
}

/**
 * @brief Read a directory entry by index. Kinds the VFS can't show (fifos, sockets, device files) are
 * skipped and not counted
 *
 * @param dir: The directory VFS node
 * @param index: The index of the entry to read
 * @param entry: Pointer to store the resulting directory entry
 * @return 1 with the entry, 0 past the last, or negative error code on failure
 */
int ext2_readdir(vfs_node_t *dir, uint64_t index, vfs_dirent_t *entry)
{
    ext2_node_t *d = EXT2_NODE(dir);
    guard(mutex, &d->lock);

    ext2_dir_cursor_t c release_with(ext2_dir_cursor_close) = ext2_dir_cursor(d);
    uint64_t count = 0;
    vfs_node_type_t type;
    while (return_on_error(ext2_dir_next(&c)))
    {
        if (!ext2_entry_type(c.entry->file_type, &type) || count++ != index)
            continue;

        entry->ino = c.entry->inode;
        entry->type = type;
        entry->length = c.entry->name_len;
        memcpy(entry->name, c.entry->name, c.entry->name_len);
        entry->name[c.entry->name_len] = '\0';
        return 1;
    }

    return 0;
}

/**
 * @brief Find a name in a directory and read what its record names. The cursor is
 * closed on return, so no lease is held after: ext2_node_get may follow (it takes
 * volume->lock and may read the disk).
 *
 * @param dir: The directory; the caller holds its lock
 * @param name: The name (not '\0'-terminated)
 * @param length: The length of the name
 * @param ino: Receives the record's inode (may be NULL)
 * @param file_type: Receives the record's EXT2_FT_* type (may be NULL)
 * @return 1 if there, 0 if not, or -errno
 */
int ext2_dir_lookup(ext2_node_t *dir, const char *name, size_t length, uint32_t *ino, uint8_t *file_type)
{
    ext2_dir_cursor_t c release_with(ext2_dir_cursor_close) = ext2_dir_cursor(dir);
    if (!return_on_error(ext2_dir_find(&c, name, length)))
        return 0;

    if (ino != NULL)
        *ino = c.entry->inode;
    if (file_type != NULL)
        *file_type = c.entry->file_type;
    return 1;
}

/**
 * @brief Look up a directory entry by name
 *
 * @param dir: The directory VFS node
 * @param name: The name of the entry to look up
 * @param length: The length of the name
 * @param child: Pointer to store the resulting child VFS node
 * @return 0, -ENOENT (also for kinds the VFS can't show), or negative error code on failure
 */
int ext2_lookup(vfs_node_t *dir, const char *name, size_t length, vfs_node_t **child)
{
    ext2_node_t *d = EXT2_NODE(dir);
    uint32_t ino = 0;
    uint8_t file_type = 0;
    int found = 0;

    scoped_guard(mutex, &d->lock)
    {
        found = return_on_error(ext2_dir_lookup(d, name, length, &ino, &file_type));
    }

    vfs_node_type_t type;
    if (!found || !ext2_entry_type(file_type, &type))
        return -ENOENT; // not there, or a kind the VFS cannot show (a fifo, socket or device file)

    // After the lock, with no lease held: ext2_node_get takes volume->lock and may read the disk
    return ext2_node_get(d->volume, ino, dir, child);
}
