#include <stdio.h>
#include <FILE.h>

int feof(FILE *f)
{
    return (f->flags & F_EOF) != 0;
}