#include <sys/stat.h>
#include <stat.h>
#include <syscall.h>

int stat(const char *restrict path, struct stat *restrict st)
{
    struct mihos_stat k;

    int ret = syscall(SYS_STAT, path, &k);
    if (ret == -1)
        return -1;

    __stat_from_kernel(st, &k);
    return 0;
}