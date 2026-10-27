#include <fs/vfs.h>
#include <fs/file.h>
#include <stddef.h>
#include <string.h>
#include <asm/memory.h>
#include <sched/sync.h>
#include <sched/spinlock.h>
#include <mm/kmalloc.h>
#include <mihos/errno.h>
#include <mihos/fcntl.h>
#include <mihos/dirent.h>
#include <mihos/stat.h>
#include <panic.h>
#include <util.h>

static vfs_node_t *vfs_root;

static uint32_t vfs_mode_type(vfs_node_type_t type)
{
    switch (type)
    {
    case VFS_DIRECTORY:
        return MIHOS_S_IFDIR;
    case VFS_CHARDEV:
        return MIHOS_S_IFCHR;
    case VFS_BLOCKDEV:
        return MIHOS_S_IFBLK;
    default:
        return MIHOS_S_IFREG;
    }
}

static uint8_t vfs_dirent_type(vfs_node_type_t type)
{
    switch (type)
    {
    case VFS_DIRECTORY:
        return MIHOS_DT_DIR;
    case VFS_CHARDEV:
        return MIHOS_DT_CHR;
    case VFS_BLOCKDEV:
        return MIHOS_DT_BLK;
    default:
        return MIHOS_DT_REG;
    }
}

const char *vfs_path_next(const char **path, size_t *length)
{
    const char *p = *path;
    while (*p == '/')
        p++;

    const char *start = p;
    while (*p != '/' && *p != '\0')
        p++;

    *length = (size_t)(p - start);
    *path = p;
    return start;
}

void vfs_node_init(vfs_node_t *node, vfs_node_type_t type, const vfs_node_ops_t *ops)
{
    node->type = type;
    node->size = 0;
    node->ops = ops;
    node->mode = 0;
    node->mtime = 0;
    node->dev = 0;
    node->mounted_root = NULL;
    node->mount_point = NULL;
    node->rdev = 0;
    node->device_ops = NULL;
    node->parent = NULL;
    node->refs = 1;
    node->nlink = 1;
    spinlock_init(&node->lock);
}

void vfs_node_get(vfs_node_t *node)
{
    scoped_guard(spinlock, &node->lock)
    {
        node->refs++;
    }
}

bool vfs_node_tryget(vfs_node_t *node)
{
    guard(spinlock, &node->lock);

    if (node->refs == 0)
        return false;

    node->refs++;
    return true;
}

void vfs_node_put(vfs_node_t *node)
{
    bool last;
    scoped_guard(spinlock, &node->lock)
    {
        last = --node->refs == 0;
    }

    if (last && node->ops && node->ops->release != NULL)
        node->ops->release(node);
}

void vfs_mount_root(vfs_node_t *root)
{
    if (vfs_root != NULL)
        panic("vfs_mount_root: Root filesystem already mounted");

    vfs_root = root;
    vfs_node_get(root);
}

uint64_t vfs_dev_alloc(void)
{
    static uint64_t next_dev = 1;
    return next_dev++;
}

int vfs_mount(const char *path, vfs_node_t *root)
{
    vfs_node_t *point;
    int ret = vfs_lookup(path, &point);
    if (ret < 0)
        return ret;

    if (point->type != VFS_DIRECTORY)
        ret = -ENOTDIR;

    else if (point == vfs_root || point->mount_point != NULL)
        ret = -EBUSY;

    if (ret < 0)
    {
        vfs_node_put(point);
        return ret;
    }

    point->mounted_root = root;
    root->mount_point = point;
    vfs_node_get(root);
    return 0;
}

int vfs_lookup(const char *path, vfs_node_t **out)
{
    if (path[0] != '/')
        return -ENOENT;

    size_t path_length = strlen(path);
    vfs_node_t *node = vfs_root;
    vfs_node_get(node);

    for (;;)
    {
        size_t length;
        const char *name = vfs_path_next(&path, &length);
        if (length == 0)
            break;

        int ret = 0;
        vfs_node_t *next = NULL;

        if (length > VFS_NAME_MAX)
            ret = -ENAMETOOLONG;
        else if (node->type != VFS_DIRECTORY)
            ret = -ENOTDIR;
        else if (length == 1 && name[0] == '.')
            continue;
        else if (length == 2 && name[0] == '.' && name[1] == '.')
        {
            vfs_node_t *up = node;
            while (up->mount_point != NULL)
                up = up->mount_point;

            next = up->parent;
            vfs_node_get(next);
        }
        else
            ret = node->ops->lookup(node, name, length, &next);

        vfs_node_put(node);

        if (ret < 0)
            return ret;

        node = next;

        // A mount point: continue in the root mounted there (several mounts may stack)
        while (node->mounted_root != NULL)
        {
            vfs_node_t *root = node->mounted_root;
            vfs_node_get(root);
            vfs_node_put(node);
            node = root;
        }
    }

    // "/usr/tests/hello/": a trailing '/' promises a directory (POSIX)
    if (path_length > 1 && path[-1] == '/' && node->type != VFS_DIRECTORY)
    {
        vfs_node_put(node);
        return -ENOTDIR;
    }

    *out = node;
    return 0;
}

long vfs_read(vfs_node_t *node, void *buffer, size_t count, uint64_t offset)
{
    if (node->type == VFS_DIRECTORY)
        return -EISDIR;
    if (node->ops->read == NULL)
        return -EINVAL; // character devices are read through their file_ops, never here

    return node->ops->read(node, buffer, count, offset);
}

long vfs_write(vfs_node_t *node, const void *buffer, size_t count, uint64_t offset)
{
    if (node->type == VFS_DIRECTORY)
        return -EISDIR;
    if (node->ops->write == NULL)
        return -EROFS; // a file on a read-only file system; character devices are written through their file_ops, never here

    return node->ops->write(node, buffer, count, offset);
}

int vfs_truncate(vfs_node_t *file, uint64_t size)
{
    if (file->type == VFS_DIRECTORY)
        return -EISDIR;
    if (file->type == VFS_CHARDEV || file->type == VFS_BLOCKDEV)
        return -EINVAL; // a device's size is the device's
    if (file->ops->truncate == NULL)
        return -EROFS; // read-only file system

    return file->ops->truncate(file, size);
}

int vfs_readdir(vfs_node_t *dir, uint64_t index, vfs_dirent_t *entry)
{
    if (dir->type != VFS_DIRECTORY)
        return -ENOTDIR;

    return dir->ops->readdir(dir, index, entry);
}

void vfs_stat(vfs_node_t *node, struct mihos_stat *stat)
{
    memset(stat, 0, sizeof(*stat));

    uint64_t size = (node->type == VFS_CHARDEV || node->type == VFS_BLOCKDEV) ? 0 : node->size;

    stat->st_ino = node->ino;
    stat->st_nlink = 1;
    stat->st_mode = vfs_mode_type(node->type) | node->mode;
    stat->st_size = size;
    stat->st_blksize = PAGE_SIZE;
    stat->st_blocks = ALIGN_UP(size, 512) / 512;
    stat->st_dev = node->dev;
    stat->st_rdev = node->rdev;
    stat->st_nlink = node->nlink;
    stat->st_atim.tv_sec = node->mtime;
    stat->st_mtim.tv_sec = node->mtime;
    stat->st_ctim.tv_sec = node->mtime;
}

int vfs_read_file(const char *path, void **data, size_t *size)
{
    vfs_node_t *node;
    int ret = vfs_lookup(path, &node);
    if (ret < 0)
        return ret;

    // Ensure the node is a file before attempting to read it
    if (node->type != VFS_FILE)
    {
        vfs_node_put(node);
        return -EISDIR;
    }

    size_t s = node->size;
    void *buffer = kmalloc(s ? s : 1);
    if (buffer == NULL)
    {
        vfs_node_put(node);
        return -ENOMEM;
    }

    size_t done = 0;
    while (done < s)
    {
        long n = vfs_read(node, (char *)buffer + done, s - done, done);
        if (n <= 0)
        {
            kfree(buffer);
            vfs_node_put(node);
            return n < 0 ? (int)n : -EIO;
        }
        done += n;
    }

    vfs_node_put(node);
    *data = buffer;
    *size = done;

    return 0;
}

long vfs_file_getdents(file_t *file, void *buffer, size_t size, uint64_t *next)
{
    uint64_t index = file->offset;
    size_t used = 0;
    vfs_dirent_t entry;

    for (;;)
    {
        int ret = vfs_readdir(file->node, index, &entry);
        if (ret < 0)
            return ret;
        if (ret == 0)
            break;

        size_t length = ALIGN_UP(offsetof(struct mihos_dirent, d_name) + entry.length + 1, 8);
        if (length > size - used)
        {
            if (used == 0)
                return -EINVAL; // not even one record fits (Linux does the same)

            // Return what has been read so far
            break;
        }

        struct mihos_dirent *record = (struct mihos_dirent *)((char *)buffer + used);
        record->d_ino = entry.ino;
        record->d_reclen = (uint16_t)length;
        record->d_type = vfs_dirent_type(entry.type);
        memcpy(record->d_name, entry.name, entry.length + 1);

        used += length;
        index++;
    }

    *next = index;
    return (long)used;
}

/**
 * Read data from a file.
 *
 * @param file The file object to read from.
 * @param buffer The buffer to store the read data.
 * @param count The number of bytes to read.
 * @return The number of bytes read on success, or a negative error code on failure.
 */
static long vfs_file_read(file_t *file, char *buffer, size_t count)
{
    // Acquire the position lock to ensure exclusive access to the file's offset.
    guard(mutex, &file->position_lock);

    long n = vfs_read(file->node, buffer, count, file->offset);
    if (n > 0)
        file->offset += (uint64_t)n;

    return n;
}

/**
 * Write data to a file.
 *
 * @param file The file object to write to.
 * @param buffer The buffer containing the data to write.
 * @param count The number of bytes to write.
 * @return The number of bytes written on success, or a negative error code on failure.
 */
static long vfs_file_write(file_t *file, const char *buffer, size_t count)
{
    guard(mutex, &file->position_lock);

    long n = vfs_write(file->node, buffer, count, file->offset);
    if (n > 0)
        file->offset += (uint64_t)n;

    return n;
}

static const file_ops_t vfs_file_ops = {
    .read = vfs_file_read,
    .write = vfs_file_write,
    .release = NULL,
};

/**
 * Create a regular file at a path whose last component does not exist yet: the parent
 * is looked up (through mounts) and asked to create the name in it.
 *
 * @param path The absolute path of the file to create.
 * @param mode The permissions for the new file (07777).
 * @param out Receives the new file's node, as a new reference.
 * @return 0, -EISDIR for a path ending in '/', -ENAMETOOLONG, the parent's lookup error,
 * -ENOTDIR, -EROFS (the parent's file system cannot create), -EEXIST if the name was
 * made meanwhile, or the file system's error.
 */
static int vfs_create(const char *path, uint32_t mode, vfs_node_t **out)
{
    size_t end = strlen(path);
    if (path[end - 1] == '/')
        return -EISDIR; // "create a directory?" (Linux's answer too); vfs_open's lookup failed, so the path is "/x" or longer

    // The last component is the name (pointing into path); the rest, its trailing '/' kept, is the parent
    size_t start = end;
    while (start > 0 && path[start - 1] != '/')
        start--;

    if (end - start > VFS_NAME_MAX)
        return -ENAMETOOLONG;

    char *parent_path = kmalloc(start + 1);
    if (parent_path == NULL)
        return -ENOMEM;
    memcpy(parent_path, path, start);
    parent_path[start] = '\0';

    vfs_node_t *dir;
    int ret = vfs_lookup(parent_path, &dir);
    kfree(parent_path);
    if (ret < 0)
        return ret;

    if (dir->type != VFS_DIRECTORY)
    {
        vfs_node_put(dir);
        return -ENOTDIR;
    }

    if (dir->ops->create == NULL)
    {
        vfs_node_put(dir);
        return -EROFS;
    }

    // The new node holds its own reference to the parent (its "..")
    ret = dir->ops->create(dir, path + start, end - start, mode & 07777, out);
    vfs_node_put(dir);
    return ret;
}

int vfs_open(const char *path, int flags, uint32_t mode, file_t **out)
{
    if (flags & ~(O_ACCMODE | O_DIRECTORY | O_TRUNC | O_CREAT | O_EXCL))
        return -EINVAL;

    vfs_node_t *node;
    int ret = vfs_lookup(path, &node);
    if (ret == -ENOENT && (flags & O_CREAT))
    {
        ret = vfs_create(path, mode, &node);

        // Someone made it between our lookup and the create: without O_EXCL, open theirs. Only
        // once: a name the VFS cannot show (a symlink) is -EEXIST to create and -ENOENT to lookup
        if (ret == -EEXIST && !(flags & O_EXCL))
            ret = vfs_lookup(path, &node);
    }
    else if (ret == 0 && (flags & O_CREAT) && (flags & O_EXCL))
    {
        vfs_node_put(node);
        return -EEXIST;
    }
    if (ret < 0)
        return ret;

    // O_DIRECTORY: opendir must not succeed on a file
    if ((flags & O_DIRECTORY) && node->type != VFS_DIRECTORY)
    {
        vfs_node_put(node);
        return -ENOTDIR;
    }

    // The access the open file gets: O_ACCMODE is a value, not bits (O_RDONLY is 0)
    unsigned int access;
    switch (flags & O_ACCMODE)
    {
    case O_RDONLY:
        access = FILE_READ;
        break;
    case O_WRONLY:
        access = FILE_WRITE;
        break;
    case O_RDWR:
        access = FILE_READ | FILE_WRITE;
        break;
    default:
        vfs_node_put(node);
        return -EINVAL;
    }

    file_t *file;
    if (node->type == VFS_CHARDEV)
    {
        // A device: its driver's ops, read and/or write as asked
        file = file_create(node->device_ops, access, NULL);
    }
    else
    {
        // Files, directories and disks: read through the node; written only if its file system can
        if (access & FILE_WRITE)
        {
            int refused = node->type == VFS_DIRECTORY ? -EISDIR : node->ops->write == NULL ? -EROFS
                                                                                           : 0;
            if (refused < 0)
            {
                vfs_node_put(node);
                return refused;
            }
        }

        file = file_create(&vfs_file_ops, access, NULL);
    }

    if (file == NULL)
    {
        vfs_node_put(node);
        return -ENOMEM;
    }

    // O_TRUNC: a regular file opened for writing starts empty (ignored without write access, and on devices)
    if ((flags & O_TRUNC) && (access & FILE_WRITE) && node->type == VFS_FILE)
    {
        int ret = vfs_truncate(node, 0);
        if (ret < 0)
        {
            file_put(file);
            return ret;
        }
    }

    file->node = node;
    *out = file;
    return 0;
}