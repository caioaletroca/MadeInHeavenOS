#include <stdlib.h>
#include <malloc.h>
#include <string.h>

void *realloc(void *ptr, size_t size)
{
    if (ptr == NULL)
        return malloc(size);

    if (size == 0)
    {
        free(ptr);
        return NULL;
    }

    block_t *block = (block_t *)ptr - 1;
    size_t capacity = (block->size - 1) * UNIT;

    // Shrinking, or growing within the rounding slack: nothing to move
    if (size <= capacity)
        return ptr;

    void *new_ptr = malloc(size);
    if (new_ptr == NULL)
        return NULL; // C requires the old block to stay valid

    memcpy(new_ptr, ptr, capacity);
    free(ptr);
    return new_ptr;
}