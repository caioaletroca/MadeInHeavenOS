#include "internal.h"
#include <mm/kmalloc.h>
#include <mihos/stat.h>
#include <mihos/errno.h>
#include <cleanup.h>
#include <util.h>

// ---- A file's status: stat and lstat by path, fstat by fd; readlink ----

/**
 * The status of the file at a path: stat follows a symlink in the last name, lstat
 * reports the link itself.
 *
 * @param path The user-space pointer to the file path.
 * @param statbuf The user-space pointer to the buffer where the file status will be stored.
 * @param follow Whether a symlink as the last name is followed.
 * @return 0 on success, or a negative error code on failure.
 */
static long stat_path(uintptr_t path, uintptr_t statbuf, bool follow)
{
    long error;
    char *kernel_path release_with(kfree_ref) = user_path_copy(path, &error);
    if (kernel_path == NULL)
        return error;

    vfs_node_t *node release_with(vfs_node_put_ref) = NULL;
    return_on_error(follow ? vfs_lookup(kernel_path, &node) : vfs_lookup_link(kernel_path, &node));

    struct mihos_stat st;
    vfs_stat(node, &st);
    if (address_space_write(syscall_space(), statbuf, &st, sizeof(st)) != 0)
        return -EFAULT;

    return 0;
}

/**
 * Retrieves the status of a file specified by its path.
 *
 * @param path The user-space pointer to the file path.
 * @param statbuf The user-space pointer to the buffer where the file status will be stored.
 * @return 0 on success, or a negative error code on failure.
 */
long sys_stat(uintptr_t path, uintptr_t statbuf)
{
    return stat_path(path, statbuf, true);
}

long sys_lstat(uintptr_t path, uintptr_t statbuf)
{
    return stat_path(path, statbuf, false);
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

/**
 * readlink: a symlink's target into the user's buffer, cut to size, with no '\0' (POSIX).
 *
 * @param path The user-space pointer to the link's path (the last name not followed).
 * @param buffer The user-space buffer.
 * @param size The buffer's size.
 * @return The bytes written, -EINVAL (size not positive, or not a symlink), -EFAULT, or
 *         the lookup's error.
 */
long sys_readlink(uintptr_t path, uintptr_t buffer, long size)
{
    if (size <= 0)
        return -EINVAL;

    long error;
    char *kernel_path release_with(kfree_ref) = user_path_copy(path, &error);
    if (kernel_path == NULL)
        return error;

    vfs_node_t *link release_with(vfs_node_put_ref) = NULL;
    return_on_error(vfs_lookup_link(kernel_path, &link));
    if (link->type != VFS_SYMLINK)
        return -EINVAL;

    size_t length = MIN((size_t)size, link->size);
    char *target release_with(kfree_ref) = kmalloc(length > 0 ? length : 1);
    if (target == NULL)
        return -ENOMEM;

    long copied = vfs_readlink(link, target, length);
    if (copied < 0)
        return copied;
    if (address_space_write(syscall_space(), buffer, target, copied) != 0)
        return -EFAULT;
    return copied;
}
