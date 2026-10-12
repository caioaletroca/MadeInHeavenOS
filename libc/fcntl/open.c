#include <fcntl.h>
#include <syscall.h>

int open(const char *path, int flags, ...)
{
    return syscall(SYS_OPEN, path, flags);
}