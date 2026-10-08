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

    if (f->pos == f->len && __fillbuf(f) != 0)
        return EOF;

    return f->buffer[f->pos++];
}