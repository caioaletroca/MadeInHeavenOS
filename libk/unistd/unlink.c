#include <unistd.h>
#include <syscall.h>

int unlink(const char *path)
{
    return syscall(SYS_UNLINK, path);
}