#include <FILE.h>
#include <stdlib.h>
#include <unistd.h>

int fclose(FILE *stream)
{
    int result = __stream_sync(stream);
    if (close(stream->fd) != 0)
        result = EOF;

    // Unlink: link points at the slot holding a stream (the head, then each next)
    for (FILE **link = &__stdio_head; *link != NULL; link = &(*link)->next)
    {
        if (*link == stream)
        {
            *link = stream->next;
            break;
        }
    }

    // Closed whatever happened (C): freed, or for the three static ones, neither readable nor writable
    if (stream->flags & F_STATIC)
        stream->flags = F_STATIC;
    else
        free(stream);

    return result;
}