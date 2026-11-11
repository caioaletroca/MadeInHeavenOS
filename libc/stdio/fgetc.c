#include <stdio.h>
#include <FILE.h>

int fgetc(FILE *f)
{
    if (f->ungot != EOF)
    {
        int c = f->ungot;
        f->ungot = EOF;
        return c;
    }

    if (f->mode != MODE_READ || f->pos == f->len)
    {
        if (__fillbuf(f) != 0)
            return EOF;
    }

    return f->buffer[f->pos++];
}