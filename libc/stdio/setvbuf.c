#include <FILE.h>
#include <errno.h>

int setvbuf(FILE *stream, char *buffer, int mode, size_t size)
{
    unsigned int how;
    if (mode == _IOFBF)
        how = 0;
    else if (mode == _IOLBF)
        how = F_LINEBUF;
    else if (mode == _IONBF)
        how = F_NOBUF;
    else
    {
        errno = EINVAL;
        return EOF;
    }

    if (buffer != NULL && mode != _IONBF && size == 0)
    {
        errno = EINVAL;
        return EOF;
    }

    // C allows setvbuf only before the first read or write; here whatever waits is synced first
    if (__stream_sync(stream) != 0)
        return EOF;

    // the caller's memory, for as long as the stream lives
    if (buffer != NULL && mode != _IONBF)
    {
        stream->buffer = (unsigned char *)buffer;
        stream->size = size;
    }
    // no buffer given (or unbuffered, which needs only a byte of it): our own
    else
    {
        stream->buffer = stream->own_buffer;
        stream->size = BUFSIZ;
    }

    // An explicit choice: F_TTYCHECK must not undo it at the first write
    stream->flags = (stream->flags & ~(F_LINEBUF | F_NOBUF | F_TTYCHECK)) | how;
    return 0;
}