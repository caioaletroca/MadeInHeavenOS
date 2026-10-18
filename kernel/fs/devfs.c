#include <fs/devfs.h>
#include <fs/vfs.h>
#include <mm/kmalloc.h>
#include <string.h>
#include <panic.h>
#include <mihos/errno.h>
#include <sys/list.h>

typedef struct devfs_node
{
    vfs_node_t node;
    char *name;
    size_t name_length;
    list_t sibling;
} devfs_node_t;

#define DEVFS_NODE(n) list_container(n, devfs_node_t, node)

static vfs_node_t devfs_root;
static uint64_t devfs_dev;
static list_t devfs_devices;
static uint64_t devfs_next_ino = 2;

static int devfs_lookup(vfs_node_t *dir, const char *name, size_t length, vfs_node_t **child);
static int devfs_readdir(vfs_node_t *dir, uint64_t index, vfs_dirent_t *entry);

static const vfs_node_ops_t devfs_ops = {
    .lookup = devfs_lookup,
    .read = NULL,
    .readdir = devfs_readdir,
    .release = NULL,
};

static const vfs_node_ops_t devfs_device_ops = {
    .lookup = NULL,
    .read = NULL,
    .readdir = NULL,
    .release = NULL,
};

static devfs_node_t *devfs_find(const char *name, size_t length)
{
    list_t *pos;
    list_for_each(pos, &devfs_devices)
    {
        devfs_node_t *n = list_container(pos, devfs_node_t, sibling);
        if (n->name_length == length && memcmp(n->name, name, length) == 0)
            return n;
    }

    return NULL;
}

void devfs_init(void)
{
    devfs_dev = vfs_dev_alloc();
    vfs_node_init(&devfs_root, VFS_DIRECTORY, &devfs_ops);

    devfs_root.dev = devfs_dev;
    devfs_root.ino = 1;
    devfs_root.mode = 0755;
    devfs_root.parent = &devfs_root;
    list_init(&devfs_devices);

    int ret = vfs_mount("/dev", &devfs_root);
    if (ret < 0)
        panic("devfs: cannot mount /dev: %d\n", ret);
}

int devfs_register(const char *name, uint32_t mode, uint64_t rdev, const struct file_ops *ops)
{
    if (name == NULL)
        return -EINVAL;

    size_t length = strlen(name);
    if (length == 0 || length > VFS_NAME_MAX || strchr(name, '/') != NULL)
        return -EINVAL;

    if (devfs_find(name, length) != NULL)
        return -EEXIST;

    devfs_node_t *n = kzalloc(sizeof(*n));
    char *copy = kmalloc(length + 1);
    if (n == NULL || copy == NULL)
    {
        kfree(n);
        kfree(copy);
        return -ENOMEM;
    }
    memcpy(copy, name, length + 1);

    vfs_node_init(&n->node, VFS_CHARDEV, &devfs_device_ops);
    n->node.dev = devfs_dev;
    n->node.ino = devfs_next_ino++;
    n->node.mode = mode & 07777;
    n->node.rdev = rdev;
    n->node.device_ops = ops;
    n->node.parent = &devfs_root;
    n->name = copy;
    n->name_length = length;
    list_insert_before(&devfs_devices, &n->sibling);

    return 0;
}

/**
 * Lookup a device node by name within the /dev directory.
 * Returns 0 if found, -ENOENT otherwise.
 */
static int devfs_lookup(vfs_node_t *dir, const char *name, size_t length, vfs_node_t **child)
{
    (void)dir;

    devfs_node_t *n = devfs_find(name, length);
    if (n == NULL)
    {
        return -ENOENT;
    }

    vfs_node_get(&n->node);
    *child = &n->node;
    return 0;
}

/**
 * Read a directory entry from the /dev directory.
 * Returns 1 if an entry is read, 0 if no more entries are available.
 */
static int devfs_readdir(vfs_node_t *dir, uint64_t index, vfs_dirent_t *entry)
{
    const char *name;
    size_t length;
    uint64_t ino;
    vfs_node_type_t type;

    if (index < 2)
    {
        name = index == 0 ? "." : "..";
        length = index + 1;
        ino = dir->ino;
        type = VFS_DIRECTORY;
    }
    else
    {
        devfs_node_t *found = NULL;
        uint64_t i = 2;
        list_t *pos;
        list_for_each(pos, &devfs_devices)
        {
            if (i++ == index)
            {
                found = list_container(pos, devfs_node_t, sibling);
                break;
            }
        }

        // After the loop: inside it, a miss on the first device would end the listing
        if (found == NULL)
            return 0;

        name = found->name;
        length = found->name_length;
        ino = found->node.ino;
        type = VFS_CHARDEV;
    }

    entry->ino = ino;
    entry->type = type;
    entry->length = length;
    memcpy(entry->name, name, length);
    entry->name[length] = '\0';

    return 1;
}