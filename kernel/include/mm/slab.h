#ifndef _SLAB_H_
#define _SLAB_H_

#include <stddef.h>
#include <sys/list.h>

/**
 * Represents a slab cache, which manages multiple slabs for objects of a specific size.
 */
typedef struct slab_cache
{
    const char *name;

    size_t object_size;
    size_t alignment;
    size_t stride;
    size_t capacity;

    list_t empty_slabs;
    list_t partial_slabs;
    list_t full_slabs;
} slab_cache_t;

/**
 * Initializes a slab cache.
 *
 * @param cache The slab cache to initialize.
 * @param name The name of the slab cache.
 * @param object_size The size of each object in the cache.
 * @param alignment The alignment requirement for objects.
 * @return 0 on success, or -1 if an error occurs.
 */
int slab_cache_init(slab_cache_t *cache, const char *name, size_t object_size, size_t alignment);

/**
 * Allocates an object from the given slab cache.
 *
 * @param cache The slab cache to allocate from.
 * @return A pointer to the allocated object, or NULL if allocation fails.
 */
void *slab_cache_alloc(slab_cache_t *cache);

/**
 * Frees an object back to the given slab cache.
 *
 * @param cache The slab cache to free the object to.
 * @param object The object to be freed.
 * @return 0 on success, or -1 if an error occurs.
 */
int slab_cache_free(slab_cache_t *cache, void *object);

#endif