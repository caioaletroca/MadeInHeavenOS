#include <fs/root.h>
#include <fs/vfs.h>
#include <fs/ext2.h>
#include <fs/initrd.h>
#include <mihos/errno.h>
#include <kprintf.h>
#include <panic.h>

/**
 * @brief Prints an error message and panics when the root file system cannot be mounted.
 *
 * @param name The name of the root device.
 * @param error The error code.
 */
static void root_panic(const char *name, int error)
{
    kprintf("root: cannot mount root=%s: %d\n", name, error);
    kprintf("root: pick root= from the block devices there are:\n");

    for (size_t i = 0; block_at(i) != NULL; i++)
    {
        const block_device_t *dev = block_at(i);
        kprintf("  %s %u KiB\n", dev->name, (unsigned int)(dev->sector_count / 2));
    }

    panic("root: unable to mount the root file system\n");
}

block_device_t *root_init(const kernel_args_t *args, const boot_info_t *boot_info)
{
    vfs_node_t *root;
    block_device_t *disk = NULL;

    if (args->root[0] == '\0')
    {
        root = initrd_init(boot_info);
        if (root == NULL)
            panic("root: no root= and no initrd\n");

        kprintf("root: the initrd mounted on /\n");
    }
    else
    {
        disk = block_find(args->root);
        int ret = disk != NULL ? ext2_mount(disk, &root) : -ENODEV;
        if (ret < 0)
            root_panic(args->root, ret);

        kprintf("root: %s mounted on /\n", disk->name);
    }

    vfs_mount_root(root);
    vfs_node_put(root);
    return disk;
}