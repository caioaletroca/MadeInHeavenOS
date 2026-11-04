#include "internal.h"
#include <mihos/stat.h>

// ---- A file's status: stat by path, fstat by fd ----

/**
 * Retrieves the status of a file specified by its path.
 *
 * @param path The user-space pointer to the file path.
 * @param statbuf The user-space pointer to the buffer where the file status will be stored.
 * @return 0 on success, or a negative error code on failure.
 */
long sys_stat(uintptr_t path, uintptr_t statbuf)
{
    long error;
    char *kernel_path = user_path_copy(path, &error);
    if (kernel_path == NULL)
        return error;

    vfs_node_t *node;
    int ret = vfs_lookup(kernel_path, &node);
    kfree(kernel_path);
    if (ret < 0)
        return ret;

    struct mihos_stat st;
    vfs_stat(node, &st);
    vfs_node_put(node);

    if (address_space_write(syscall_space(), statbuf, &st, sizeof(st)) != 0)
        return -EFAULT;

    return 0;
}

/**
 * Retrieves the status of an open file specified by its file descriptor.
 *
 * @param fd The file descriptor of the open file.
 * @param statbuf The user-space pointer to the buffer where the file status will be stored.
 * @return 0 on success, or a negative error code on failure.
 */
long sys_fstat(unsigned long fd, uintptr_t statbuf)
{
    file_t *file = sys_file(fd);
    if (file == NULL)
        return -EBADF;

    struct mihos_stat st;
    vfs_stat(file->node, &st);
    if (address_space_write(syscall_space(), statbuf, &st, sizeof(st)) != 0)
        return -EFAULT;

    return 0;
}
