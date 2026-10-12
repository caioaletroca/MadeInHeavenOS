#include <unistd.h>
#include <syscall.h>

off_t lseek(int fd, off_t offset, int whence)
{
    return syscall(SYS_LSEEK, fd, offset, whence);
}