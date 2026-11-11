#include <FILE.h>

void rewind(FILE *stream)
{
    (void)fseek(stream, 0, SEEK_SET);
    stream->flags &= ~F_ERR;
}