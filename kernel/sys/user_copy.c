#include "internal.h"

// ---- Copies from user memory: strings and paths, through the caller's address space ----

/**
 * Returns the address space of the currently executing thread.
 */
address_space_t *syscall_space(void)
{
    return scheduler_current()->space;
}

/**
 * user_string_copy(address, buffer, size): copies a null-terminated string from user space to the kernel buffer.
 * Returns the number of bytes copied on success, or a negative error code on failure.
 */
long user_string_copy(uintptr_t address, char *buffer, size_t size)
{
    for (size_t i = 0; i < size; i++)
    {
        if (address_space_read(syscall_space(), address + i, &buffer[i], 1) != 0)
            return -EFAULT;

        if (buffer[i] == '\0')
            return (long)i;
    }
    return -E2BIG;
}

/**
 * Copies a user path into a kmalloc'd VFS_PATH_MAX buffer.
 *
 * @param path The user-space path to copy.
 * @param error Pointer to a long where the error code will be stored if the copy fails.
 * @return The buffer (the caller kfrees it), or NULL with *error set
 *         (-ENOMEM, -EFAULT, -ENAMETOOLONG).
 */
char *user_path_copy(uintptr_t path, long *error)
{
    // On the heap: VFS_PATH_MAX (4 KiB) is a quarter of a kernel stack
    char *buffer = kmalloc(VFS_PATH_MAX);
    if (buffer == NULL)
    {
        *error = -ENOMEM;
        return NULL;
    }

    long ret = user_string_copy(path, buffer, VFS_PATH_MAX);
    if (ret == -E2BIG)
        ret = -ENAMETOOLONG;

    if (ret < 0)
    {
        kfree(buffer);
        *error = ret;
        return NULL;
    }

    return buffer;
}
