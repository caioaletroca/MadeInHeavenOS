#include <sys/mman.h>
#include <syscall.h>

int mprotect(void *address, size_t length, int protection)
{
    return syscall(SYS_MPROTECT, address, length, protection);
}