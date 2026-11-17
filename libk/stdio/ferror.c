#include <FILE.h>
#include <stdio.h>

int ferror(FILE *stream)
{
    return (stream->flags & F_ERR) != 0;
}
