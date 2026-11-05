#include "internal.h"
#include <mihos/ioctl.h>
#include <fs/pipe.h>

// ---- Calls on open files: open, read, write, seek, close, dup, ioctl, truncate ----

// Bytes moved per copy; bounds the kernel stack buffer
#define SYSCALL_CHUNK_SIZE 256
#define IOCTL_ARG_MAX 256

/**
 * sys_file(fd): returns the file structure associated with the given file descriptor for the current process.
 */
file_t *sys_file(unsigned long fd)
{
    process_t *p = scheduler_current()->process;
    return fd < PROCESS_MAX_FILES ? p->files[fd] : NULL;
}

/**
 * write(fd, buffer, size): writes data to the given file descriptor from the specified buffer.
 */
long sys_write(unsigned long fd, uintptr_t buffer, size_t size)
{
    char chunk[SYSCALL_CHUNK_SIZE];
    size_t done = 0;

    file_t *file = sys_file(fd);
    if (file == NULL || !(file->flags & FILE_WRITE) || file->ops->write == NULL)
        return -EBADF;

    while (done < size)
    {
        size_t length = size - done;

        if (length > SYSCALL_CHUNK_SIZE)
            length = SYSCALL_CHUNK_SIZE;

        // A fault after some output reports what was written, like Linux
        if (address_space_read(syscall_space(), buffer + done, chunk, length) != 0)
            return done > 0 ? (long)done : -EFAULT;

        long status = file->ops->write(file, chunk, length);
        if (status < 0)
        {
            return done > 0 ? (long)done : status;
        }

        done += (size_t)status;
        if ((size_t)status < length)
            break;
    }

    return (long)done;
}

/**
 * read(fd, buffer, size): reads data from the given file descriptor into the specified buffer.
 */
long sys_read(unsigned long fd, uintptr_t buffer, size_t size)
{
    char chunk[SYSCALL_CHUNK_SIZE];

    file_t *file = sys_file(fd);
    if (file == NULL || !(file->flags & FILE_READ) || file->ops->read == NULL)
        return -EBADF;

    if (size > SYSCALL_CHUNK_SIZE)
        size = SYSCALL_CHUNK_SIZE;

    long length = file->ops->read(file, chunk, size);
    if (length < 0)
        return length;

    if (address_space_write(syscall_space(), buffer, chunk, length) != 0)
        return -EFAULT;

    return length;
}

long sys_close(unsigned long fd)
{
    process_t *p = scheduler_current()->process;

    file_t *file = sys_file(fd);
    if (file == NULL)
        return -EBADF;

    p->files[fd] = NULL;
    file_put(file);
    return 0;
}

/**
 * dup(fd): duplicates the given file descriptor, returning a new descriptor.
 */
long sys_dup(unsigned long fd)
{
    file_t *file = sys_file(fd);
    if (file == NULL)
        return -EBADF;

    file_get(file);
    int ret = process_fd_install(scheduler_current()->process, file);
    if (ret < 0)
        file_put(file);

    return ret;
}

/**
 * dup2(oldfd, newfd): duplicates the given file descriptor to the specified new descriptor.
 */
long sys_dup2(unsigned long oldfd, unsigned long newfd)
{
    process_t *p = scheduler_current()->process;

    file_t *file = sys_file(oldfd);
    if (file == NULL || newfd >= PROCESS_MAX_FILES)
        return -EBADF;

    // POSIX: the same fd is left alone (closing it first would drop the only reference)
    if (oldfd == newfd)
        return (long)newfd;

    file_t *old = p->files[newfd];
    p->files[newfd] = file_get(file);
    p->close_on_exec &= ~(1u << newfd);
    if (old != NULL)
        file_put(old);

    return (long)newfd;
}

/**
 * Opens a file specified by the path and flags.
 *
 * @param path The user-space pointer to the file path.
 * @param flags The flags for opening the file (e.g., O_RDONLY).
 * @param mode The permissions of a file created by O_CREAT; ignored otherwise.
 * @return The file descriptor on success, or a negative error code on failure.
 */
long sys_open(uintptr_t path, int flags, uint32_t mode)
{
    process_t *current = scheduler_current()->process;

    long error;
    char *buffer = user_path_copy(path, &error);
    if (buffer == NULL)
        return error;

    file_t *file = NULL;
    long ret = vfs_open(buffer, flags, mode, &file);

    kfree(buffer);
    if (ret < 0)
        return ret;

    int fd = process_fd_install(current, file);
    if (fd < 0)
        file_put(file);

    return fd;
}

/**
 * Repositions the read/write file offset of the open file associated with the file descriptor.
 *
 * @param fd The file descriptor of the open file.
 * @param offset The offset to set the file position to.
 * @param whence The reference point for the offset (SEEK_SET, SEEK_CUR, SEEK_END).
 * @return The resulting offset location as measured in bytes from the beginning of the file, or a negative error code on failure.
 */
long sys_lseek(unsigned long fd, long offset, int whence)
{
    file_t *file = sys_file(fd);
    if (file == NULL)
        return -EBADF;
    if (file->node == NULL || file->node->type == VFS_CHARDEV || file->node->type == VFS_FIFO)
        return -ESPIPE;

    // Lock the file's position to ensure thread-safe seeking.
    guard(mutex, &file->position_lock);

    uint64_t base;
    switch (whence)
    {
    case SEEK_SET:
        base = 0;
        break;
    case SEEK_CUR:
        base = file->offset;
        break;
    case SEEK_END:
        base = file->node->size;
        break;
    default:
        return -EINVAL;
    }

    if (offset < 0 && (uint64_t)-(offset + 1) >= base)
        return -EINVAL;

    uint64_t new = base + offset;
    file->offset = new;

    return (long)new;
}

long sys_ioctl(unsigned long fd, unsigned long request, uintptr_t arg)
{
    file_t *file = sys_file(fd);
    if (file == NULL)
        return -EBADF;

    if (file->ops->ioctl == NULL)
        return -ENOTTY;

    // Determine the size of the ioctl argument based on the request code.
    size_t size = MIHOS_IOC_SIZE(request);
    if (size > IOCTL_ARG_MAX)
        return -EINVAL;

    char buffer[IOCTL_ARG_MAX];
    memset(buffer, 0, IOCTL_ARG_MAX);

    // If the ioctl command expects data to be written from user space, read it into the buffer.
    if ((MIHOS_IOC_DIR(request) & MIHOS_IOC_WRITE) && size > 0)
    {
        if (address_space_read(syscall_space(), arg, buffer, size) != 0)
            return -EFAULT;
    }

    // Call the ioctl handler with the prepared buffer.
    long ret = file->ops->ioctl(file, request, (void *)buffer);

    // If the ioctl command expects data to be read into user space, write it from the buffer.
    if (ret >= 0 && (MIHOS_IOC_DIR(request) & MIHOS_IOC_READ) && size > 0)
    {
        if (address_space_write(syscall_space(), arg, buffer, size) != 0)
            return -EFAULT;
    }

    return ret;
}

/**
 * Truncate an open file to a specified length.
 *
 * @param fd The file descriptor of the open file.
 * @param length The new length of the file.
 * @return 0 on success, or a negative error code on failure.
 */
long sys_ftruncate(unsigned long fd, long length)
{
    file_t *file = sys_file(fd);
    if (file == NULL)
        return -EBADF;

    if (!(file->flags & FILE_WRITE))
        return -EINVAL;

    if (file->node == NULL)
        return -EINVAL;

    if (length < 0)
        return -EINVAL;

    return vfs_truncate(file->node, (uint64_t)length);
}

/**
 * Truncate a file specified by its path to a given length.
 *
 * @param path The path to the file.
 * @param length The new length of the file.
 * @return 0 on success, or a negative error code on failure.
 */
long sys_truncate(uintptr_t path, long length)
{
    if (length < 0)
        return -EINVAL;

    long error;
    char *kernel_path = user_path_copy(path, &error);
    if (kernel_path == NULL)
        return error;

    vfs_node_t *node;
    int ret = vfs_lookup(kernel_path, &node);
    kfree(kernel_path);
    if (ret < 0)
        return ret;

    ret = vfs_truncate(node, (uint64_t)length);
    vfs_node_put(node);

    return ret;
}

/**
 * Create a pipe and return file descriptors for the read and write ends.
 *
 * @param fds Pointer to an array of two integers where the file descriptors will be stored.
 * @return 0 on success, or a negative error code on failure.
 */
long sys_pipe(uintptr_t fds)
{
    process_t *p = scheduler_current()->process;

    file_t *read_end, *write_end;
    int ret = pipe_create(&read_end, &write_end);
    if (ret < 0)
        return ret;

    int pair[2];
    pair[0] = process_fd_install(p, read_end);
    if (pair[0] < 0)
    {
        file_put(read_end);
        file_put(write_end);
        return pair[0]; // return the error code from process_fd_install for the read end
    }
    pair[1] = process_fd_install(p, write_end);
    if (pair[1] < 0)
    {
        sys_close(pair[0]);
        file_put(write_end);
        return pair[1]; // return the error code from process_fd_install for the write end
    }

    if (address_space_write(syscall_space(), fds, pair, sizeof(pair)) != 0)
    {
        sys_close(pair[0]);
        sys_close(pair[1]);
        return -EFAULT;
    }

    return 0;
}

/**
 * Perform operations on a file descriptor.
 *
 * @param fd The file descriptor.
 * @param command The command to perform (e.g., F_GETFD, F_SETFD).
 * @param arg The argument for the command.
 * @return The flags for F_GETFD, 0 for F_SETFD, or a negative error code on failure.
 */
long sys_fcntl(unsigned long fd, int command, long arg)
{
    process_t *p = scheduler_current()->process;

    if (sys_file(fd) == NULL)
        return -EBADF;

    uint32_t bit = 1u << fd;

    switch (command)
    {
    case F_GETFD:
        return (p->close_on_exec & bit) != 0 ? FD_CLOEXEC : 0;
    case F_SETFD:
        if (arg & FD_CLOEXEC)
            p->close_on_exec |= bit;
        else
            p->close_on_exec &= ~bit;
        return 0;
    default:
        return -EINVAL;
    }
}