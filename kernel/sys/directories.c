#include "internal.h"
#include <mm/kmalloc.h>
#include <cleanup.h>

// ---- Calls on directories and names: listing, unlink, mkdir, rmdir, rename, symlink, the current directory ----

long sys_getdents(unsigned long fd, uintptr_t buffer, size_t size)
{
    file_t *file = sys_file(fd);
    if (file == NULL)
        return -EBADF;
    if (file->node == NULL)
        return -ENOTDIR;

    size = MIN(size, PAGE_SIZE);

    char *records = kzalloc(size ? size : 1);
    if (records == NULL)
        return -ENOMEM;

    long ret;
    scoped_guard(mutex, &file->position_lock)
    {
        uint64_t next;
        ret = vfs_file_getdents(file, records, size, &next);
        if (ret > 0 && address_space_write(syscall_space(), buffer, records, (size_t)ret) != 0)
            ret = -EFAULT;
        if (ret >= 0)
            file->offset = next;
    }

    kfree(records);
    return ret;
}

/**
 * Remove a file specified by its path.
 *
 * @param path The path to the file to remove.
 * @return 0 on success, or a negative error code on failure.
 */
long sys_unlink(uintptr_t path)
{
    long error;
    char *kernel_path = user_path_copy(path, &error);
    if (kernel_path == NULL)
        return error;

    int ret = vfs_unlink(kernel_path);
    kfree(kernel_path);
    return ret;
}

/**
 * Create a directory at the specified path.
 *
 * @param path The path to the directory to create.
 * @param mode The permissions for the new directory (07777).
 * @return 0 on success, or a negative error code on failure.
 */
long sys_mkdir(uintptr_t path, uint32_t mode)
{
    long error;
    char *kernel_path = user_path_copy(path, &error);
    if (kernel_path == NULL)
        return error;

    int ret = vfs_mkdir(kernel_path, mode);
    kfree(kernel_path);
    return ret;
}

/**
 * Remove a directory at the specified path.
 *
 * @param path The path to the directory to remove.
 * @return 0 on success, or a negative error code on failure.
 */
long sys_rmdir(uintptr_t path)
{
    long error;
    char *kernel_path = user_path_copy(path, &error);
    if (kernel_path == NULL)
        return error;

    int ret = vfs_rmdir(kernel_path);
    kfree(kernel_path);
    return ret;
}

/**
 * rename(old, new): gives the file or directory at old the name new, replacing
 * what new names (see vfs_rename for the rules).
 *
 * @param old_path The user-space pointer to the current path.
 * @param new_path The user-space pointer to the new path.
 * @return 0 on success, or a negative error code on failure.
 */
long sys_rename(uintptr_t old_path, uintptr_t new_path)
{
    long error;
    char *old_kernel_path = user_path_copy(old_path, &error);
    if (old_kernel_path == NULL)
        return error;

    char *new_kernel_path = user_path_copy(new_path, &error);
    if (new_kernel_path == NULL)
    {
        kfree(old_kernel_path);
        return error;
    }

    int ret = vfs_rename(old_kernel_path, new_kernel_path);
    kfree(new_kernel_path);
    kfree(old_kernel_path);
    return ret;
}

/**
 * Make a symlink (symlink): link_path will hold target.
 *
 * @param target User pointer to what the link holds.
 * @param link_path User pointer to where the link goes.
 * @return 0, or what copying the strings or vfs_symlink returns.
 */
long sys_symlink(uintptr_t target, uintptr_t link_path)
{
    long error;
    char *kernel_target release_with(kfree_ref) = user_path_copy(target, &error);
    if (kernel_target == NULL)
        return error;

    char *kernel_path release_with(kfree_ref) = user_path_copy(link_path, &error);
    if (kernel_path == NULL)
        return error;

    return vfs_symlink(kernel_target, kernel_path);
}

/**
 * Change the current working directory of the calling process.
 *
 * @param path The path to the new current working directory.
 * @return 0 on success, or a negative error code on failure.
 */
long sys_chdir(uintptr_t path)
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

    if (node->type != VFS_DIRECTORY)
    {
        vfs_node_put(node);
        return -ENOTDIR;
    }

    process_cwd_set(scheduler_current()->process, node);
    return 0;
}

/**
 * Get the current working directory of the calling process.
 *
 * @param buffer The buffer to receive the path.
 * @param size The size of the buffer.
 * @return The number of bytes written on success, or a negative error code on failure.
 */
long sys_getcwd(uintptr_t buffer, size_t size)
{
    char *path = kmalloc(VFS_PATH_MAX);
    if (path == NULL)
        return -ENOMEM;

    long ret = vfs_getcwd(path, VFS_PATH_MAX);
    if (ret > 0 && (size_t)ret > size)
        ret = -ERANGE;

    if (ret > 0 && address_space_write(syscall_space(), buffer, path, (size_t)ret) != 0)
        ret = -EFAULT;

    kfree(path);
    return ret;
}
