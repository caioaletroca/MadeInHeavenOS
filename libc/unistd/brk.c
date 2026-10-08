#include <unistd.h>
#include <syscall.h>
#include <errno.h>

int brk(void *addr)
{
    if ((void *)__syscall(SYS_BRK, addr) != addr)
    {
        errno = ENOMEM;
        return -1;
    }

    return 0;
}