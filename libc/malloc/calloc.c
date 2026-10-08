#include <stdlib.h>
#include <malloc.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>

void *calloc(size_t number, size_t size)
{
    if (number == 0 || size == 0)
        return NULL;

    if (number > SIZE_MAX / size)
    {
        errno = ENOMEM;
        return NULL;
    }

    size_t total_size = number * size;
    void *ptr = malloc(total_size);
    if (ptr == NULL)
        return NULL;

    memset(ptr, 0, total_size);
    return ptr;
}