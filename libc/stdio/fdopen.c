#include <FILE.h>
#include <fcntl.h>
#include <errno.h>

FILE *fdopen(int fd, const char *mode)
{
    int open_flags;
    unsigned int flags = __mode_flags(mode, &open_flags);
    if (flags == 0)
    {
        errno = EINVAL;
        return NULL;
    }

    if (fcntl(fd, F_GETFD) < 0)
        return NULL;

    return __stream_new(fd, flags);
}