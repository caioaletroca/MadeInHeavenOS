#include <unistd.h>
#include <stdint.h>
#include <syscall.h>
#include <errno.h>

void *sbrk(intptr_t increment)
{
    uintptr_t old = (uintptr_t)__syscall(SYS_BRK, 0);
    if (increment == 0)
        return (void *)old;

    uintptr_t new = old + (uintptr_t)increment;
    if ((increment > 0 && new < old) || (increment < 0 && new > old))
    {
        errno = ENOMEM;
        return (void *)-1;
    }

    if (brk((void *)new) != 0)
        return (void *)-1;

    return (void *)old;
}