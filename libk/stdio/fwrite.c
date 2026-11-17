#include <FILE.h>
#include <stdint.h>
#include <stdio.h>

size_t fwrite(const void *ptr, size_t size, size_t count, FILE *stream)
{
    if (size == 0 || count == 0)
        return 0;

    if (count > SIZE_MAX / size)
    {
        stream->flags |= F_ERR;
        return 0;
    }

    return __fwritex((const unsigned char *)ptr, size * count, stream) / size;
}