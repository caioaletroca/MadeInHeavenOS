#include <FILE.h>
#include <unistd.h>
#include <string.h>

size_t __fwritex(const unsigned char *data, size_t n, FILE *f)
{
    // Check if the file is open for writing
    if (!(f->flags & F_WRITE))
    {
        f->flags |= F_ERR;
        return 0;
    }

    if (f->mode != MODE_WRITE)
    {
        f->pos = f->len = 0;
        f->mode = MODE_WRITE;
    }

    // Line buffering: everything up to the last '\n' must leave now
    size_t now = 0;
    if (f->flags & F_LINEBUF)
        for (size_t i = n; i > 0; i--)
            if (data[i - 1] == '\n')
            {
                now = i;
                break;
            }

    if ((f->flags & F_NOBUF) || n - now > f->size - f->pos)
        now = n;

    if (now > 0)
    {
        // If the data fits in the buffer, copy it and flush if necessary
        if (f->pos + now <= f->size)
        {
            memcpy(f->buffer + f->pos, data, now);
            f->pos += now;
            if (__fflush_one(f) != 0)
                return 0;
        }
        // If the data doesn't fit in the buffer, flush the buffer and write directly
        else
        {
            if (__fflush_one(f) != 0 || __write_all(f, data, now) != now)
                return 0;
        }
    }

    // The rest (after the last '\n', or everything when nothing had to go out) stays buffered
    size_t rest = n - now;
    if (rest > 0)
    {
        memcpy(f->buffer + f->pos, data + now, rest);
        f->pos += rest;
    }

    return n;
}