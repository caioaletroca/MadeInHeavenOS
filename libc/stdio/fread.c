#include <FILE.h>
#include <stdint.h>
#include <string.h>

size_t fread(void *ptr, size_t size, size_t count, FILE *stream)
{
    if (size == 0 || count == 0)
        return 0;

    if (count > SIZE_MAX / size)
    {
        stream->flags |= F_ERR;
        return 0;
    }

    unsigned char *out = ptr;
    size_t wanted = size * count;
    size_t done = 0;

    if (stream->ungot != EOF)
    {
        out[done++] = (unsigned char)stream->ungot;
        stream->ungot = EOF;
    }

    while (done < wanted)
    {
        // What the buffer holds goes first, then another read
        if (stream->mode == MODE_READ && stream->pos < stream->len)
        {
            size_t n = stream->len - stream->pos;
            if (n > wanted - done)
                n = wanted - done;

            memcpy(out + done, stream->buffer + stream->pos, n);
            stream->pos += n;
            done += n;
        }
        else if (__fillbuf(stream) != 0)
            break;
    }

    // Whole items only: a partial one at the end is read but not counted (C)
    return done / size;
}