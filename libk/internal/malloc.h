#ifndef _INTERNAL_MALLOC_H_
#define _INTERNAL_MALLOC_H_

#include <stddef.h>
#include <sys/list.h>

/**
 * @brief A memory block used by the malloc implementation.
 */
typedef struct block
{
    list_t link; // On the free list while free; link.next == MALLOC_MAGIC while allocated
    size_t size; // In units, header included
} __attribute__((aligned(16))) block_t;

#define UNIT sizeof(block_t)
#define MALLOC_MAGIC ((list_t *)0xA110CA7EDB10C000)

// Free blocks, sorted by address (defined in __malloc_insert.c)
// TODO: lock once user threads exist
extern list_t __malloc_free_list;

/**
 * @brief Insert a block into the free list, merging it with adjacent free blocks.
 *
 * @param block The block to insert into the free list.
 */
void __malloc_insert(block_t *block);

/**
 * @brief Get at least units more units from the kernel and add them to the free list.
 *
 * @param units The number of memory units to request from the kernel.
 * @return 0 on success, -1 if sbrk fails (errno is ENOMEM).
 */
int __morecore(size_t units);

#endif /* _INTERNAL_MALLOC_H_ */