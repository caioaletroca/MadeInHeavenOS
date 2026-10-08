#include <malloc.h>

// Sentinel node for the free list. This node is never removed and always points to itself.
list_t __malloc_free_list = {&__malloc_free_list, &__malloc_free_list};

/**
 * Checks if two memory blocks are contiguous.
 *
 * @param a The first block.
 * @param b The second block.
 * @return true if the blocks touch each other, false otherwise.
 */
static bool touches(const block_t *a, const block_t *b)
{
    return (const char *)a + a->size * UNIT == (const char *)b;
}

/**
 * Inserts a block into the free list, merging with adjacent free blocks if possible.
 *
 * @param block The block to insert.
 */
void __malloc_insert(block_t *block)
{
    // First free block above this one, or the sentinel if there is none
    list_t *pos;
    list_for_each(pos, &__malloc_free_list)
    {
        if (pos > &block->link)
        {
            break;
        }
    }

    // Before the sentinel means "at the end": same call either way
    list_insert_before(pos, &block->link);

    // Merge with the block after (the sentinel is never a block)
    if (block->link.next != &__malloc_free_list)
    {
        block_t *next = list_container(block->link.next, block_t, link);

        if (touches(block, next))
        {
            block->size += next->size;
            list_delete(&next->link);
        }
    }

    // Merge with the block before
    if (block->link.prev != &__malloc_free_list)
    {
        block_t *prev = list_container(block->link.prev, block_t, link);

        if (touches(prev, block))
        {
            prev->size += block->size;
            list_delete(&block->link);
        }
    }
}