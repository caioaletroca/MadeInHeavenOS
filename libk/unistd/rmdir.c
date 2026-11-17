#include <unistd.h>
#include <syscall.h>

int rmdir(const char *path)
{
    return syscall(SYS_RMDIR, path);
}