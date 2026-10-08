#include <FILE.h>
#include <unistd.h>

int __fillbuf(FILE *f)
{
    if (!(f->flags & F_READ))
    {
        f->flags |= F_ERR;
        return EOF;
    }

    // Sticky: after end of file, nothing is read until clearerr
    if (f->flags & F_EOF)
        return EOF;

    // The one buffer may still hold output: send it before reusing the buffer
    if (f->mode != MODE_READ)
    {
        if (__fflush_one(f) != 0)
            return EOF;
        f->mode = MODE_READ;
    }

    for (FILE *s = __stdio_head; s != NULL; s = s->next)
        if ((s->flags & F_LINEBUF) && s->mode == MODE_WRITE)
            __fflush_one(s);

    ssize_t n = read(f->fd, f->buffer, f->size);
    if (n <= 0)
    {
        f->flags |= n == 0 ? F_EOF : F_ERR;
        return EOF;
    }

    f->pos = 0;
    f->len = (size_t)n;
    return 0;
}