#include <malloc.h>
#include <unistd.h>
#include <stdint.h>
#include <errno.h>

// At least 64 KiB per sbrk, so most malloc calls never reach the kernel
#define MORECORE_MIN_UNITS (64 * 1024 / UNIT)

int __morecore(size_t units)
{
    if (units < MORECORE_MIN_UNITS)
        units = MORECORE_MIN_UNITS;

    if (units > INTPTR_MAX / UNIT)
    {
        errno = ENOMEM;
        return -1;
    }

    void *memory = sbrk((intptr_t)(units * UNIT));
    if (memory == (void *)-1)
        return -1;

    block_t *block = memory;
    block->size = units;
    __malloc_insert(block);
    return 0;
}