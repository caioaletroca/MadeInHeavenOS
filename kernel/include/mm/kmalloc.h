#ifndef _KMALLOC_H_
#define _KMALLOC_H_

#include <stddef.h>

/**
 * Initializes the kernel memory allocator.
 */
void kmalloc_init(void);

/**
 * Allocates a block of memory of the specified size.
 *
 * @param size The size of the memory block to allocate.
 * @return A pointer to the allocated memory, or NULL if allocation fails.
 */
void *kmalloc(size_t size);

/**
 * Frees a previously allocated object.
 *
 * @param ptr The pointer to the object to be freed.
 */
void kfree(void *ptr);

#endif /* _KMALLOC_H_ */