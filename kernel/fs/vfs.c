#include <fs/vfs.h>
#include <string.h>
#include <sched/spinlock.h>
#include <mm/kmalloc.h>
#include <mihos/errno.h>
#include <panic.h>

static vfs_node_t *vfs_root;

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
    node->parent = NULL;
    node->refs = 1;
    spinlock_init(&node->lock);
}

void vfs_node_get(vfs_node_t *node)
{
    scoped_guard(spinlock, &node->lock)
    {
        node->refs++;
    }
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
            next = node->parent;
            vfs_node_get(next);
        }
        else
            ret = node->ops->lookup(node, name, length, &next);

        vfs_node_put(node);

        if (ret < 0)
            return ret;

        node = next;
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
    if (node->type != VFS_FILE)
        return -EISDIR;

    return node->ops->read(node, buffer, count, offset);
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