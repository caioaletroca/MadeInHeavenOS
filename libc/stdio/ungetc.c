#include <stdio.h>
#include <FILE.h>

int ungetc(int c, FILE *f)
{
    if (c == EOF)
        return EOF;

    f->ungot = (unsigned char)c;
    f->flags &= ~F_EOF;
    return f->ungot;
}