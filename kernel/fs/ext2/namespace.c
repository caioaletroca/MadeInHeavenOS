#include "internal.h"
#include <driver/clock.h>
#include <mihos/errno.h>
#include <mm/kmalloc.h>
#include <logger.h>

// ---- Names: creating files and removing their names; a node's blocks and inode freed at its last put ----

/**
 * @brief A new, empty regular file in a directory: an inode taken and prepared, then its name
 *
 * The name comes last, so no record ever points at an unprepared inode; if it cannot be
 * added, the inode goes back. The directory's links stay (no ".."); its mtime and ctime
 * become now, the new file's three times too.
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
    int64_t now = clock_seconds();

    ext2_node_t *node = EXT2_NODE(dir);
    ext2_volume_t *volume = node->volume;

    guard(mutex, &node->lock);

    if (node->flags & EXT2_INDEX_FL)
        return -EROFS; // a hashed directory: a record added without updating its index would hide from Linux's lookups

    if (dir->nlink == 0)
        return -ENOENT; // removed (rmdir) while still held: no new names in it

    if (return_on_error(ext2_dir_lookup(node, name, length, NULL, NULL)))
        return -EEXIST; // whatever its type: a second record with the name would be corruption

    uint32_t ino = 0;
    int ret = ext2_inode_alloc(volume, node->ino, false, &ino);
    if (ret < 0)
        return ret;

    ret = ext2_inode_init(volume, ino, EXT2_S_IFREG | (mode & 07777), 1, (uint32_t)now);
    if (ret == 0)
        ret = ext2_dir_add(node, name, length, ino, EXT2_FT_REG_FILE);

    if (ret < 0)
    {
        ext2_inode_free(volume, ino, false);
        return ret;
    }

    // The directory gained a name. The name is already on disk, so a failure here
    // only leaves an old time: still hand back the file
    ext2_mark_changed(node, now);
    ext2_times_store(node);

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
        // A fast symlink's i_block is its target's text, not blocks: nothing to free there
        int ret = ext2_is_fast_symlink(n) ? 0 : ext2_truncate(node, 0);
        if (ret == 0)
            ret = ext2_inode_free(n->volume, n->ino, node->type == VFS_DIRECTORY);
        if (ret < 0)
            log_error("ext2: freeing inode %u failed (%d): run e2fsck\n", n->ino, ret);
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
 * @return 0, -ENOENT (also for kinds the VFS cannot show, like fifos), -EISDIR for a
 * directory, -EROFS for an indexed directory, -EIO for a file with no links, or -errno
 */
int ext2_unlink(vfs_node_t *dir, const char *name, size_t length)
{
    int64_t now = clock_seconds();
    ext2_node_t *d = EXT2_NODE(dir);
    ext2_volume_t *volume = d->volume;
    vfs_node_t *child = NULL;
    int ret = 0;

    scoped_guard(mutex, &d->lock)
    {
        if (d->flags & EXT2_INDEX_FL)
            ret = -EROFS; // a hashed directory: its index would still list the name

        uint32_t ino = 0;
        uint8_t file_type = 0;
        vfs_node_type_t type;
        if (ret == 0)
        {
            ret = ext2_dir_lookup(d, name, length, &ino, &file_type);
            if (ret == 0 || (ret == 1 && !ext2_entry_type(file_type, &type)))
                ret = -ENOENT;
            else if (ret == 1)
                ret = 0;
        }

        // The node before anything changes
        if (ret == 0)
            ret = ext2_node_get(volume, ino, dir, &child);
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
                child->nlink--;
                ret = ext2_links_store(c);
                if (ret == 0)
                {
                    child->ctime = now;
                    ret = ext2_times_store(c);
                }
                if (child->nlink == 0)
                    c->free_on_release = true;
            }
        }
        if (ret == 0)
        {
            ext2_mark_changed(d, now);
            ret = ext2_times_store(d);
        }
    }

    // After the directory's lock: if nobody has the file open, this put frees it right here
    if (child != NULL)
        vfs_node_put(child);

    return ret;
}

int ext2_mkdir(vfs_node_t *dir, const char *name, size_t length, uint32_t mode)
{
    int64_t now = clock_seconds();
    ext2_node_t *d = EXT2_NODE(dir);
    ext2_volume_t *volume = d->volume;
    vfs_node_t *child = NULL;
    int ret = 0;

    scoped_guard(mutex, &d->lock)
    {
        if (d->flags & EXT2_INDEX_FL)
            ret = -EROFS; // a hashed directory: its index would still list the name
        else if (dir->nlink == 0)
            ret = -ENOENT; // removed (rmdir) while still held: no new names in it
        else if (dir->nlink >= EXT2_LINK_MAX)
            ret = -EMLINK; // too many links in the directory

        if (ret == 0)
        {
            ret = ext2_dir_lookup(d, name, length, NULL, NULL);
            if (ret == 1)
                ret = -EEXIST; // entry already exists
        }

        uint32_t ino = 0;
        if (ret == 0)
            ret = ext2_inode_alloc(volume, d->ino, true, &ino);
        if (ret == 0)
        {
            ret = ext2_inode_init(volume, ino, EXT2_S_IFDIR | (mode & 07777), 2, (uint32_t)now);
            if (ret == 0)
                ret = ext2_node_get(volume, ino, dir, &child);
            if (ret < 0)
                ext2_inode_free(volume, ino, true);
        }

        if (ret == 0)
        {
            ext2_node_t *c = EXT2_NODE(child);
            scoped_guard(mutex, &c->lock)
            {
                ret = ext2_dir_add(c, ".", 1, ino, EXT2_FT_DIR);
                if (ret == 0)
                    ret = ext2_dir_add(c, "..", 2, d->ino, EXT2_FT_DIR);

                if (ret < 0)
                {
                    child->nlink = 0; // never named: its block and inode go at the put below
                    c->free_on_release = true;
                }
            }
        }

        if (ret == 0)
        {
            ret = ext2_dir_add(d, name, length, ino, EXT2_FT_DIR);
            if (ret < 0)
            {
                child->nlink = 0;
                EXT2_NODE(child)->free_on_release = true;
            }
        }
        if (ret == 0)
        {
            dir->nlink++;
            ret = ext2_links_store(d);
        }
        if (ret == 0)
        {
            ext2_mark_changed(d, now);
            ret = ext2_times_store(d);
        }
    }

    // After the parent's lock (as in ext2_unlink); a failed directory is freed here
    if (child != NULL)
        vfs_node_put(child);

    return ret;
}

int ext2_rmdir(vfs_node_t *dir, const char *name, size_t length)
{
    int64_t now = clock_seconds();
    ext2_node_t *d = EXT2_NODE(dir);
    ext2_volume_t *volume = d->volume;
    vfs_node_t *child = NULL;
    int ret = 0;

    scoped_guard(mutex, &d->lock)
    {
        if (d->flags & EXT2_INDEX_FL)
            ret = -EROFS; // a hashed directory: its index would still list the name

        uint32_t ino = 0;
        uint8_t file_type = 0;
        vfs_node_type_t type;
        if (ret == 0)
        {
            ret = ext2_dir_lookup(d, name, length, &ino, &file_type);
            if (ret == 0 || (ret == 1 && !ext2_entry_type(file_type, &type)))
                ret = -ENOENT;
            else if (ret == 1)
                ret = 0;
        }

        if (ret == 0)
            ret = ext2_node_get(volume, ino, dir, &child);
        if (ret == 0 && child->type != VFS_DIRECTORY)
            ret = -ENOTDIR; // unlink's
        if (ret == 0 && child->nlink == 0)
            ret = -EIO; // a name for a file with no links: the disk is corrupt

        // Empty, then removed, with nothing in between (a create would need this lock)
        if (ret == 0)
        {
            ext2_node_t *c = EXT2_NODE(child);
            scoped_guard(mutex, &c->lock)
            {
                ret = ext2_dir_empty(c);
                if (ret == 0)
                    ret = -ENOTEMPTY;
                else if (ret == 1)
                    ret = ext2_dir_remove(d, name, length);

                // Both its links go at once (its name and its "."); at 0 the last put frees it
                if (ret == 0)
                {
                    child->nlink = 0;
                    c->free_on_release = true;
                    ret = ext2_links_store(c);
                }
            }
        }

        // The parent loses the link of the child's ".."
        if (ret == 0)
        {
            dir->nlink--;
            ret = ext2_links_store(d);
        }
        if (ret == 0)
        {
            ext2_mark_changed(d, now);
            ret = ext2_times_store(d);
        }
    }

    // After the parent's lock: if nobody holds the directory, this put frees it right here
    if (child != NULL)
        vfs_node_put(child);

    return ret;
}

// ---- Renames: one at a time per volume (rename_lock), so they alone hold two directories ----

/**
 * @brief Whether a node is a given directory or lies somewhere under it
 *
 * Walks the parents up to the volume's root (its own parent). Only renames change a
 * parent and the caller holds rename_lock, so the chain holds still; every node on it
 * is kept alive by its child's reference, so no lock is needed per step.
 *
 * @param node: Where the walk starts
 * @param ancestor: The directory looked for
 * @return true if node is ancestor or one of its descendants
 */
static bool ext2_is_under(vfs_node_t *node, vfs_node_t *ancestor)
{
    for (;;)
    {
        if (node == ancestor)
            return true;
        if (node->parent == node)
            return false;
        node = node->parent;
    }
}

/**
 * @brief Take two directories' locks without a deadlock: an ancestor's before its
 * descendant's, the order every other operation follows (a directory before its child)
 *
 * Two unrelated directories go in any order: only a rename ever holds two, and
 * rename_lock (held by the caller) lets one run at a time. The same directory twice is
 * locked once. Used through the ext2_dirs guard.
 *
 * @param a: One directory
 * @param b: The other (may be a)
 */
static void ext2_lock_two(ext2_node_t *a, ext2_node_t *b)
{
    if (a == b)
    {
        mutex_lock(&a->lock);
        return;
    }
    if (ext2_is_under(&a->base, &b->base))
    {
        ext2_node_t *swap = a;
        a = b;
        b = swap;
    }
    mutex_lock(&a->lock);
    mutex_lock(&b->lock);
}

/**
 * @brief Release what ext2_lock_two took (the order of unlocking does not matter)
 *
 * @param a: One directory
 * @param b: The other (may be a: then it is unlocked once)
 */
static void ext2_unlock_two(ext2_node_t *a, ext2_node_t *b)
{
    mutex_unlock(&a->lock);
    if (a != b)
        mutex_unlock(&b->lock);
}

/**
 * @brief The guard for ext2_lock_two: guard(ext2_dirs, a, b) locks both, ancestor first,
 * and unlocks them at the end of the scope
 */
typedef struct ext2_dirs_guard
{
    ext2_node_t *a, *b;
} ext2_dirs_guard_t;

static inline ext2_dirs_guard_t ext2_dirs_guard_init(ext2_node_t *a, ext2_node_t *b)
{
    ext2_lock_two(a, b);
    return (ext2_dirs_guard_t){.a = a, .b = b};
}

static inline void ext2_dirs_guard_exit(ext2_dirs_guard_t *guard)
{
    ext2_unlock_two(guard->a, guard->b);
}

/**
 * @brief A rename's name, found and its node taken
 *
 * @param dir: The directory; the caller holds its lock
 * @param name: The name (not '\0'-terminated)
 * @param length: The length of the name
 * @param unshown: The error for a name of a kind the VFS can't show (a fifo, socket or device file)
 * @param node: Receives the node, as a new reference
 * @return 1 if found, 0 if not there, unshown, or -errno
 */
static int ext2_rename_get(ext2_node_t *dir, const char *name, size_t length, int unshown, vfs_node_t **node)
{
    uint32_t ino;
    uint8_t file_type;
    vfs_node_type_t type;

    if (!return_on_error(ext2_dir_lookup(dir, name, length, &ino, &file_type)))
        return 0;
    if (!ext2_entry_type(file_type, &type))
        return unshown;
    return_on_error(ext2_node_get(dir->volume, ino, &dir->base, node));
    return 1;
}

/**
 * @brief Every reason a rename is refused, before anything is written
 *
 * @param old_dir: The source's directory
 * @param new_dir: The directory it moves to (may be old_dir)
 * @param source: The entry renamed
 * @param target: What the new name names now, or NULL
 * @return 0 if it may go on, or the refusal
 */
static int ext2_rename_check(vfs_node_t *old_dir, vfs_node_t *new_dir, vfs_node_t *source, vfs_node_t *target)
{
    bool directory = source->type == VFS_DIRECTORY;

    if (new_dir->nlink == 0)
        return -ENOENT; // the new directory was removed while held
    if (directory && ext2_is_under(new_dir, source))
        return -EINVAL; // into itself or its own subtree

    if (target == NULL)
    {
        if (directory && old_dir != new_dir && new_dir->nlink >= EXT2_LINK_MAX)
            return -EMLINK; // its ".." would be one link too many
        return 0;
    }

    if (ext2_is_under(old_dir, target))
        return -ENOTEMPTY; // the target holds the source
    if (directory && target->type != VFS_DIRECTORY)
        return -ENOTDIR;
    if (!directory && target->type == VFS_DIRECTORY)
        return -EISDIR;
    if (target->type != VFS_DIRECTORY)
        return 0;

    ext2_node_t *t = EXT2_NODE(target);
    guard(mutex, &t->lock);
    if (!return_on_error(ext2_dir_empty(t)))
        return -ENOTEMPTY;
    return 0;
}

/**
 * @brief A directory that changes parent: its ".." names the new one, and the link
 * that ".." is moves with it
 *
 * @param source: The directory moved
 * @param d: Its old parent
 * @param nd: Its new parent
 * @return 0 or -errno
 */
static int ext2_rename_move_dotdot(vfs_node_t *source, ext2_node_t *d, ext2_node_t *nd)
{
    ext2_node_t *s = EXT2_NODE(source);
    scoped_guard(mutex, &s->lock)
    {
        return_on_error(ext2_dir_set(s, "..", 2, nd->ino, EXT2_FT_DIR));
    }
    d->base.nlink--;
    nd->base.nlink++;
    return 0;
}

/**
 * @brief The replaced target loses its name: a file one link, a directory both (its
 * name and its "."), and then its parent loses the link of its ".."; at 0 it is freed
 * at its last put
 *
 * @param target: The node replaced
 * @param nd: The directory that held its name
 * @param now: The rename's time
 * @return 0 or -errno
 */
static int ext2_rename_drop_target(vfs_node_t *target, ext2_node_t *nd, int64_t now)
{
    ext2_node_t *t = EXT2_NODE(target);
    bool directory = target->type == VFS_DIRECTORY;

    scoped_guard(mutex, &t->lock)
    {
        target->nlink = directory ? 0 : target->nlink - 1;
        target->ctime = now;
        if (target->nlink == 0)
            t->free_on_release = true;
        return_on_error(ext2_links_store(t));
    }
    if (directory)
        nd->base.nlink--;
    return 0;
}

/**
 * @brief Store what a rename changed: the source's ctime, each directory's link
 * count, mtime and ctime
 *
 * @param source: The entry renamed
 * @param d: The old directory
 * @param nd: The new directory (may be d)
 * @param now: The rename's time
 * @return 0 or -errno
 */
static int ext2_rename_store(vfs_node_t *source, ext2_node_t *d, ext2_node_t *nd, int64_t now)
{
    ext2_node_t *s = EXT2_NODE(source);
    scoped_guard(mutex, &s->lock)
    {
        source->ctime = now;
        return_on_error(ext2_times_store(s));
    }

    return_on_error(ext2_links_store(d));
    ext2_mark_changed(d, now);
    return_on_error(ext2_times_store(d));

    if (nd != d)
    {
        return_on_error(ext2_links_store(nd));
        ext2_mark_changed(nd, now);
        return_on_error(ext2_times_store(nd));
    }
    return 0;
}

/**
 * @brief Give an entry a new name, in its directory or another one of the volume,
 * replacing what the new name names (POSIX rename)
 *
 * Every check comes before any write. Then the new name first (added, or the target's
 * record pointed at the source in place), a moved directory's ".." rewritten in place,
 * the old name last: a crash in between leaves two names, never none (e2fsck mends the
 * link count). A replaced target loses its name and is freed at its last put; the link
 * counts change in memory and are stored once. The source's node then moves under its
 * new parent (vfs_node_set_parent), still under rename_lock.
 *
 * @param old_dir: The directory holding the entry
 * @param old_name: The entry's name (not '\0'-terminated)
 * @param old_length: The length of old_name
 * @param new_dir: The directory it moves to (may be old_dir)
 * @param new_name: The new name (not '\0'-terminated)
 * @param new_length: The length of new_name
 * @return 0 (also when both names are one file: nothing changes), -ENOENT, -EEXIST for a
 * target the VFS can't show (a fifo, socket or device file), -EINVAL (a directory into its own subtree),
 * -ENOTEMPTY (a non-empty target, or one holding the source), -ENOTDIR, -EISDIR,
 * -EMLINK, -EROFS for an indexed directory, or -errno
 */
int ext2_rename(vfs_node_t *old_dir, const char *old_name, size_t old_length,
                vfs_node_t *new_dir, const char *new_name, size_t new_length)
{
    int64_t now = clock_seconds();
    ext2_node_t *d = EXT2_NODE(old_dir);
    ext2_node_t *nd = EXT2_NODE(new_dir);
    bool moves = d != nd;

    // Released in reverse order: the two directories, then the nodes, then rename_lock
    guard(mutex, &d->volume->rename_lock);
    vfs_node_t *source release_with(vfs_node_put_ref) = NULL;
    vfs_node_t *target release_with(vfs_node_put_ref) = NULL;

    if ((d->flags | nd->flags) & EXT2_INDEX_FL)
        return -EROFS;

    {
        guard(ext2_dirs, d, nd);

        if (!return_on_error(ext2_rename_get(d, old_name, old_length, -ENOENT, &source)))
            return -ENOENT;
        return_on_error(ext2_rename_get(nd, new_name, new_length, -EEXIST, &target));

        if (target != NULL && EXT2_NODE(target)->ino == EXT2_NODE(source)->ino)
            return 0; // the same file under both names: nothing to do (POSIX)

        return_on_error(ext2_rename_check(old_dir, new_dir, source, target));

        // The writes: the new name first, so a crash leaves two names, never none
        bool directory = source->type == VFS_DIRECTORY;
        uint32_t ino = EXT2_NODE(source)->ino;
        uint8_t file_type = ext2_file_type(source->type);

        if (target == NULL)
            return_on_error(ext2_dir_add(nd, new_name, new_length, ino, file_type));
        else
            return_on_error(ext2_dir_set(nd, new_name, new_length, ino, file_type));

        if (directory && moves)
            return_on_error(ext2_rename_move_dotdot(source, d, nd));
        return_on_error(ext2_dir_remove(d, old_name, old_length));
        if (target != NULL)
            return_on_error(ext2_rename_drop_target(target, nd, now));
        return_on_error(ext2_rename_store(source, d, nd, now));
    }

    // Only on success, still under rename_lock: the next rename's ancestor checks see the new parent
    if (moves)
        vfs_node_set_parent(source, new_dir);
    return 0;
}