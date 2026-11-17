#include <FILE.h>
#include <unistd.h>
#include <errno.h>

int __stream_sync(FILE *f)
{
    int result = 0;

    if (f->mode == MODE_WRITE)
        result = __fflush_one(f);
    else
    {
        off_t ahead = (off_t)(f->len - f->pos) + (f->ungot != EOF);
        if (ahead > 0)
        {
            int saved = errno;
            lseek(f->fd, -ahead, SEEK_CUR);
            errno = saved;
        }
    }

    f->pos = f->len = 0;
    f->ungot = EOF;
    f->mode = MODE_NONE;
    return result;
}