#include <dirent.h>
#include <dirstream.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

DIR *opendir(const char *path)
{
    int fd = open(path, O_RDONLY | O_DIRECTORY);
    if (fd == -1)
        return NULL;

    DIR *dir = malloc(sizeof(DIR));
    if (dir == NULL)
    {
        close(fd);
        errno = ENOMEM;
        return NULL;
    }

    dir->fd = fd;
    dir->pos = 0;
    dir->len = 0;
    return dir;
}