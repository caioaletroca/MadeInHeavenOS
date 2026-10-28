#include "internal.h"
#include <mihos/errno.h>
#include <mm/kmalloc.h>
#include <kprintf.h>

// ---- Names: creating files and removing their names; a node's blocks and inode freed at its last put ----

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
