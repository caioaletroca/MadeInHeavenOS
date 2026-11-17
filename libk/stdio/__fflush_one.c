#include <FILE.h>

int __fflush_one(FILE *f)
{
    if (f->mode != MODE_WRITE || f->pos == 0)
        return 0;

    size_t pending = f->pos;
    size_t done = __write_all(f, f->buffer, pending);

    // On error, the buffer is discarded anyway
    f->pos = 0;
    return done == pending ? 0 : EOF;
}