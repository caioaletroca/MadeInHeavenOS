#include <unistd.h>
#include <syscall.h>

int pipe(int fds[2])
{
    return syscall(SYS_PIPE, (uintptr_t)fds);
}