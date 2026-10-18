#include <fs/devfs.h>
#include <fs/vfs.h>
#include <string.h>
#include <panic.h>
#include <mihos/errno.h>

static vfs_node_t devfs_root;
static uint64_t devfs_dev;

static int devfs_lookup(vfs_node_t *dir, const char *name, size_t length, vfs_node_t **child);
static int devfs_readdir(vfs_node_t *dir, uint64_t index, vfs_dirent_t *entry);

static const vfs_node_ops_t devfs_ops = {
    .lookup = devfs_lookup,
    .read = NULL,
    .readdir = devfs_readdir,
    .release = NULL,
};

void devfs_init(void)
{
    devfs_dev = vfs_dev_alloc();
    vfs_node_init(&devfs_root, VFS_DIRECTORY, &devfs_ops);

    devfs_root.dev = devfs_dev;
    devfs_root.ino = 1;
    devfs_root.mode = 0755;
    devfs_root.parent = &devfs_root;

    int ret = vfs_mount("/dev", &devfs_root);
    if (ret < 0)
        panic("devfs: cannot mount /dev: %d\n", ret);
}

static int devfs_lookup(vfs_node_t *dir, const char *name, size_t length, vfs_node_t **child)
{
    (void)dir;
    (void)name;
    (void)length;
    (void)child;
    return -ENOENT; // Not implemented
}

static int devfs_readdir(vfs_node_t *dir, uint64_t index, vfs_dirent_t *entry)
{
    if (index > 1)
        return 0;

    const char *name = index == 0 ? "." : "..";
    entry->type = VFS_DIRECTORY;
    entry->ino = dir->ino;
    entry->length = index + 1;
    memcpy(entry->name, name, entry->length + 1);
    return 1;
}