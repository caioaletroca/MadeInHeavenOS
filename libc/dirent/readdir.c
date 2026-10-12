#include <dirent.h>
#include <dirstream.h>
#include <string.h>

struct dirent *readdir(DIR *dir)
{
    // Buffer used up: fetch the next batch of records from the kernel
    if (dir->pos >= dir->len)
    {
        ssize_t n = getdents(dir->fd, dir->buffer, sizeof(dir->buffer));
        if (n <= 0)
            return NULL;

        dir->len = (size_t)n;
        dir->pos = 0;
    }

    const struct mihos_dirent *record = (const void *)(dir->buffer + dir->pos);
    dir->pos += record->d_reclen;

    dir->entry.d_ino = record->d_ino;
    dir->entry.d_type = record->d_type;
    strlcpy(dir->entry.d_name, record->d_name, sizeof(dir->entry.d_name));

    return &dir->entry;
}