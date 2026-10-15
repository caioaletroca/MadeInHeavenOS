#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>

int isatty(int fd)
{
    struct stat st;
    if (fstat(fd, &st) != 0)
        return 0;

    if (!S_ISCHR(st.st_mode))
    {
        errno = ENOTTY;
        return 0;
    }

    return 1;
}