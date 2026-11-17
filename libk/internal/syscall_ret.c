#include <syscall.h>
#include <errno.h>

long __syscall_ret(unsigned long result)
{
    if (result > -4096UL)
    {
        errno = -(long)result;
        return -1;
    }

    return result;
}