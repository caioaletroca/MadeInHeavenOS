#include <dirent.h>
#include <dirstream.h>
#include <unistd.h>

void rewinddir(DIR *dir)
{
    lseek(dir->fd, 0, SEEK_SET);
    dir->pos = 0;
    dir->len = 0;
}