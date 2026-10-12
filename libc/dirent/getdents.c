#include <dirent.h>
#include <syscall.h>

ssize_t getdents(int fd, void *buffer, size_t size)
{
    return syscall(SYS_GETDENTS, fd, buffer, size);
}