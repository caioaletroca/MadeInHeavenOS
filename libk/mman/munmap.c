#include <sys/mman.h>
#include <syscall.h>

int munmap(void *address, size_t length)
{
    return syscall(SYS_MUNMAP, address, length);
}