#include <fs/devfs.h>
#include <fs/vfs.h>
#include <driver/block.h>
#include <mm/kmalloc.h>
#include <string.h>
#include <panic.h>
#include <mihos/errno.h>
#include <sys/list.h>

typedef struct devfs_node
{
    vfs_node_t base;
    char *name;
    size_t name_length;
    list_t device_link;
    block_device_t *block; // block devices: what reads go to
} devfs_node_t;

#define DEVFS_NODE(n) list_container(n, devfs_node_t, base)

static vfs_node_t devfs_root;
static uint64_t devfs_dev;
static list_t devfs_devices;
static uint64_t devfs_next_ino = 2;

static int devfs_lookup(vfs_node_t *dir, const char *name, size_t length, vfs_node_t **child);
static int devfs_readdir(vfs_node_t *dir, uint64_t index, vfs_dirent_t *entry);
static long devfs_block_read(vfs_node_t *node, void *buffer, size_t count, uint64_t offset);
static long devfs_block_write(vfs_node_t *node, const void *buffer, size_t count, uint64_t offset);

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

static const vfs_node_ops_t devfs_block_ops = {
    .lookup = NULL,
    .read = devfs_block_read,
    .write = devfs_block_write,
    .readdir = NULL,
    .release = NULL,
};

/**
 * @brief Finds a device node by name within the /dev directory.
 *
 * @param name The name of the device node to find.
 * @param length The length of the name.
 * @return Pointer to the found device node, or NULL if not found.
 */
static devfs_node_t *devfs_find(const char *name, size_t length)
{
    list_t *pos;
    list_for_each(pos, &devfs_devices)
    {
        devfs_node_t *n = list_container(pos, devfs_node_t, device_link);
        if (n->name_length == length && memcmp(n->name, name, length) == 0)
            return n;
    }

    return NULL;
}

/**
 * @brief Adds a new device node to the /dev directory.
 *
 * @param name The name of the device node.
 * @param type The type of the device node.
 * @param mode The mode (permissions) of the device node.
 * @param rdev The device number.
 * @param ops The operations for the device node.
 * @param out Pointer to store the created device node.
 * @return 0 on success, -EINVAL if the name is invalid, -EEXIST if the device already exists, -ENOMEM on memory allocation failure.
 */
static int devfs_add(const char *name, vfs_node_type_t type, uint32_t mode, uint64_t rdev, const vfs_node_ops_t *ops, devfs_node_t **out)
{
    if (name == NULL)
        return -EINVAL;

    size_t length = strlen(name);
    if (length == 0 || length > VFS_NAME_MAX || strchr(name, '/') != NULL)
        return -EINVAL;

    if (devfs_find(name, length) != NULL)
        return -EEXIST;

    devfs_node_t *dev = kzalloc(sizeof(devfs_node_t));
    char *copy = kmalloc(length + 1);
    if (dev == NULL || copy == NULL)
    {
        kfree(dev);
        kfree(copy);
        return -ENOMEM;
    }
    memcpy(copy, name, length + 1);

    vfs_node_init(&dev->base, type, ops);
    dev->name = copy;
    dev->name_length = length;
    list_insert_before(&devfs_devices, &dev->device_link);

    dev->base.dev = devfs_dev;
    dev->base.ino = devfs_next_ino++;
    dev->base.mode = mode & 07777;
    dev->base.rdev = rdev;
    dev->base.parent = &devfs_root;

    *out = dev;

    return 0;
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
}

void devfs_mount(void)
{
    int ret = vfs_mount("/dev", &devfs_root);
    if (ret < 0)
        panic("devfs: cannot mount /dev: %d\n", ret);
}

int devfs_register(const char *name, uint32_t mode, uint64_t rdev, const struct file_ops *ops)
{
    devfs_node_t *n;
    int ret = devfs_add(name, VFS_CHARDEV, mode, rdev, &devfs_device_ops, &n);
    if (ret < 0)
        return ret;

    n->base.device_ops = ops;

    return 0;
}

int devfs_register_block(const char *name, uint32_t mode, block_device_t *block)
{
    devfs_node_t *n;
    int ret = devfs_add(name, VFS_BLOCKDEV, mode, block->rdev, &devfs_block_ops, &n);
    if (ret < 0)
        return ret;

    n->block = block;
    n->base.size = n->block->sector_count * BLOCK_SECTOR_SIZE;

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

    vfs_node_get(&n->base);
    *child = &n->base;
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
                found = list_container(pos, devfs_node_t, device_link);
                break;
            }
        }

        // After the loop: inside it, a miss on the first device would end the listing
        if (found == NULL)
            return 0;

        name = found->name;
        length = found->name_length;
        ino = found->base.ino;
        type = found->base.type;
    }

    entry->ino = ino;
    entry->type = type;
    entry->length = length;
    memcpy(entry->name, name, length);
    entry->name[length] = '\0';

    return 1;
}

/**
 * Read data from a block device node.
 * @param node The block device node.
 * @param buffer The buffer to read data into.
 * @param count The number of bytes to read.
 * @param offset The offset within the block device to start reading from.
 * @return The number of bytes read, or a negative error code on failure.
 */
static long devfs_block_read(vfs_node_t *node, void *buffer, size_t count, uint64_t offset)
{
    return block_read_bytes(DEVFS_NODE(node)->block, buffer, count, offset);
}

/**
 * Write data to a block device node.
 * @param node The block device node.
 * @param buffer The buffer containing data to write.
 * @param count The number of bytes to write.
 * @param offset The offset within the block device to start writing to.
 * @return The number of bytes written, or a negative error code on failure.
 */
static long devfs_block_write(vfs_node_t *node, const void *buffer, size_t count, uint64_t offset)
{
    return block_write_bytes(DEVFS_NODE(node)->block, buffer, count, offset);
}