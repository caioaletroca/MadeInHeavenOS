#include <FILE.h>

int fileno(FILE *stream)
{
    return stream->fd;
}