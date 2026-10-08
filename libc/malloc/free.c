#include <stdlib.h>
#include <stdio.h>
#include <malloc.h>

void free(void *ptr)
{
    if (ptr == NULL)
        return;

    // The header sits right before the memory malloc handed out
    block_t *block = (block_t *)ptr - 1;

    if (block->link.next != MALLOC_MAGIC)
    {
        fputs("free: invalid pointer or double free\n", stderr);
        abort();
    }

    __malloc_insert(block);
}