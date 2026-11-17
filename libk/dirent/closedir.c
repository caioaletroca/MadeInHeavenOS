#include <dirent.h>
#include <dirstream.h>
#include <unistd.h>
#include <stdlib.h>

int closedir(DIR *dir)
{
    int ret = close(dir->fd);
    free(dir);
    return ret;
}