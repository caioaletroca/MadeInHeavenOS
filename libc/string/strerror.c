#include <string.h>
#include <errno.h>

/**
 * Array of error messages corresponding to error codes.
 */
static const char *const messages[] = {
    [0] = "No error",
    [EPERM] = "Operation not permitted",
    [ENOENT] = "No such file or directory",
    [EIO] = "Input/output error",
    [E2BIG] = "Argument list too long",
    [ENOEXEC] = "Exec format error",
    [EBADF] = "Bad file descriptor",
    [ECHILD] = "No child processes",
    [ENOMEM] = "Out of memory",
    [EACCES] = "Permission denied",
    [EFAULT] = "Bad address",
    [EBUSY] = "Device or resource busy",
    [EEXIST] = "File exists",
    [ENOTDIR] = "Not a directory",
    [EISDIR] = "Is a directory",
    [EINVAL] = "Invalid argument",
    [EMFILE] = "Too many open files",
    [ENOTTY] = "Not a terminal",
    [EFBIG] = "File too large",
    [ENOSPC] = "No space left on device",
    [ESPIPE] = "Illegal seek",
    [EROFS] = "Read-only file system",
    [ENAMETOOLONG] = "File name too long",
    [ENOSYS] = "Function not implemented",
    [ENOBUFS] = "No buffer space available",
};

char *strerror(int error)
{
    if (error >= 0 && (size_t)error < sizeof(messages) / sizeof(messages[0]) && messages[error] != NULL)
        return (char *)messages[error];

    return (char *)"Unknown error";
}