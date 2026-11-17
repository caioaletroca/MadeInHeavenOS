#include <unistd.h>
#include <syscall.h>

ssize_t read(int fd, void *buf, size_t count)
{
    return syscall(SYS_READ, fd, buf, count);
}