#include <FILE.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

FILE *fopen(const char *path, const char *mode)
{
    int open_flags;
    unsigned int flags = __mode_flags(mode, &open_flags);
    if (flags == 0)
    {
        errno = EINVAL;
        return NULL;
    }

    int fd = open(path, open_flags, 0666);
    if (fd < 0)
        return NULL;

    FILE *f = __stream_new(fd, flags);
    if (f == NULL)
    {
        int saved = errno;
        close(fd);
        errno = saved;
    }

    return f;
}