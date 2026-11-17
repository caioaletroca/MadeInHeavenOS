#include <FILE.h>
#include <unistd.h>

long ftell(FILE *stream)
{
    // Pending output first: in "a" mode the kernel puts it at the end, so only after it is the end known
    if (stream->mode == MODE_WRITE && __fflush_one(stream) != 0)
        return -1;

    off_t position = lseek(stream->fd, 0, SEEK_CUR);
    if (position < 0)
        return -1;

    // The fd read ahead: the stream is behind it by what is still unread (and an ungetc byte)
    if (stream->mode == MODE_READ)
        position -= (off_t)(stream->len - stream->pos);
    if (stream->ungot != EOF)
        position--;

    return position;
}