#include <sys/stat.h>
#include <stat.h>
#include <syscall.h>

int fstat(int fd, struct stat *st)
{
    struct mihos_stat k;

    int ret = syscall(SYS_FSTAT, fd, &k);
    if (ret == -1)
        return -1;

    __stat_from_kernel(st, &k);
    return 0;
}