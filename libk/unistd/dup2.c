#include <unistd.h>
#include <syscall.h>

int dup2(int oldfd, int newfd)
{
    return syscall(SYS_DUP2, oldfd, newfd);
}