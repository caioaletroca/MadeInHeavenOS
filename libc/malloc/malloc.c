#include <stdlib.h>
#include <stdint.h>
#include <malloc.h>
#include <errno.h>

/**
 * Take the first block that fits the requested number of units.
 * Returns NULL if no suitable block is found.
 */
static block_t *take_first_fit(size_t units)
{
    list_t *pos;
    list_for_each(pos, &__malloc_free_list)
    {
        block_t *block = list_container(pos, block_t, link);
        if (block->size < units)
            continue;

        if (block->size == units)
        {
            list_delete(&block->link);
            return block;
        }

        block->size -= units;
        block_t *tail = block + block->size;
        tail->size = units;
        return tail;
    }

    return NULL;
}

void *malloc(size_t size)
{
    if (size == 0)
        return NULL;

    if (size > SIZE_MAX - UNIT)
    {
        errno = ENOMEM;
        return NULL;
    }

    size_t units = (size + UNIT - 1) / UNIT + 1;

    block_t *block = take_first_fit(units);
    if (block == NULL)
    {
        if (__morecore(units) != 0)
            return NULL;

        block = take_first_fit(units);
    }

    block->link.next = MALLOC_MAGIC;
    return block + 1;
}