#include <FILE.h>
#include <unistd.h>
#include <errno.h>

int fseek(FILE *stream, long offset, int whence)
{
    if (whence != SEEK_SET && whence != SEEK_CUR && whence != SEEK_END)
    {
        errno = EINVAL;
        return -1;
    }

    // Asked before anything is dropped: a failed fseek leaves the stream as it was
    if (lseek(stream->fd, 0, SEEK_CUR) < 0)
        return -1;

    // The fd at the stream's position, so SEEK_CUR counts from where the program is, not the fd
    if (__stream_sync(stream) != 0)
        return -1;

    if (lseek(stream->fd, offset, whence) < 0)
        return -1;

    // C: a successful fseek clears end of file, and undoes ungetc
    stream->flags &= ~F_EOF;
    return 0;
}