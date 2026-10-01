#include <assert.h>
#include <mm/slab.h>
#include <mm/frame.h>
#include <asm/memory.h>
#include <kmalloc.h>
#include <util.h>

/**
 * Represents a free object within a slab.
 */
typedef struct slab_free_object
{
    struct slab_free_object *next;
} slab_free_object_t;

/**
 * Represents the state of a slab within the cache.
 */
typedef enum slab_state
{
    SLAB_EMPTY,
    SLAB_PARTIAL,
    SLAB_FULL
} slab_state_t;

/**
 * Represents a single slab within a slab cache.
 */
typedef struct slab
{
    list_t link;

    void *memory;
    physaddr_t physical;

    slab_free_object_t *free_objects;

    size_t capacity;
    size_t inuse;

    slab_state_t state;
} slab_t;

/**
 * Checks if the given object belongs to the specified slab within the cache.
 *
 * @param cache The slab cache containing the slab.
 * @param slab The slab to check.
 * @param object The object to check for membership.
 * @return true if the object belongs to the slab, false otherwise.
 */
static bool slab_contains(const slab_cache_t *cache, const slab_t *slab, const void *object)
{
    uintptr_t start = (uintptr_t)slab->memory;
    uintptr_t address = (uintptr_t)object;
    uintptr_t end = start + slab->capacity * cache->stride;
    return address >= start && address < end && ((address - start) % cache->stride == 0);
}

/**
 * Finds the slab in the given list that contains the specified object.
 *
 * @param head The head of the list of slabs to search.
 * @param cache The slab cache containing the slabs.
 * @param object The object to find the owning slab for.
 * @return A pointer to the slab containing the object, or NULL if not found.
 */
static slab_t *slab_list_find(list_t *head, const slab_cache_t *cache, const void *object)
{
    list_t *node;
    list_for_each(node, head)
    {
        slab_t *slab = list_container(node, slab_t, link);
        if (slab_contains(cache, slab, object))
            return slab;
    }
    return NULL;
}

/**
 * Checks if the given object is currently free within the specified slab.
 *
 * @param slab The slab containing the object.
 * @param object The object to check.
 * @return true if the object is free, false otherwise.
 */
static bool slab_object_is_free(const slab_t *slab, const void *object)
{
    size_t visited = 0;
    for (const slab_free_object_t *obj = slab->free_objects; obj != NULL; obj = obj->next)
    {
        if (obj == object)
            return true;
        visited++;

        KASSERT(visited <= slab->capacity);
    }
    return false;
}

/**
 * Validates the integrity of a slab within the cache.
 *
 * @param cache The slab cache containing the slab.
 * @param slab The slab to validate.
 * @return true if the slab is valid, false otherwise.
 */
static bool slab_validate(const slab_cache_t *cache, const slab_t *slab)
{
    size_t free_count = 0;

    for (const slab_free_object_t *object = slab->free_objects; object != NULL; object = object->next)
    {
        if (!slab_contains(cache, slab, object))
            return false;
        free_count++;

        if (free_count > slab->capacity)
            return false;
    }

    if (free_count + slab->inuse != slab->capacity)
        return false;

    if (slab->inuse > slab->capacity)
        return false;

    return true;
}

int slab_cache_init(slab_cache_t *cache, const char *name, size_t object_size, size_t alignment)
{
    if (cache == NULL || object_size == 0)
        return -1;

    if (alignment == 0)
        alignment = sizeof(void *);

    // Check if the alignment is a power of two
    if ((alignment & (alignment - 1)) != 0)
        return -1;

    size_t minimum = object_size;

    if (minimum < sizeof(slab_free_object_t))
        minimum = sizeof(slab_free_object_t);

    size_t stride = ALIGN_UP(minimum, alignment);

    // TODO: Consider handling cases where the stride exceeds the page size more gracefully.
    if (stride > PAGE_SIZE)
        return -1;

    cache->name = name;
    cache->object_size = object_size;
    cache->alignment = alignment;
    cache->stride = stride;
    cache->capacity = PAGE_SIZE / cache->stride;

    if (cache->capacity == 0)
        return -1;

    list_init(&cache->empty_slabs);
    list_init(&cache->partial_slabs);
    list_init(&cache->full_slabs);

    KASSERT(cache->stride >= cache->object_size);
    KASSERT(cache->stride >= sizeof(slab_free_object_t));
    KASSERT(cache->capacity > 0);
    KASSERT(cache->capacity * cache->stride <= PAGE_SIZE);
    KASSERT((cache->alignment & (cache->alignment - 1)) == 0);

    return 0;
}

/**
 * Builds the free object list for a given slab.
 */
static void slab_build_free_objects(const slab_cache_t *cache, slab_t *slab)
{
    char *memory = slab->memory;

    for (size_t i = 0; i < slab->capacity; i++)
    {
        slab_free_object_t *object = (slab_free_object_t *)(memory + i * cache->stride);
        object->next = slab->free_objects;
        slab->free_objects = object;
    }
}

/**
 * Creates a new slab for the given slab cache.
 *
 * @param cache The slab cache to create the slab for.
 * @return A pointer to the newly created slab, or NULL if creation fails.
 */
static slab_t *slab_create(slab_cache_t *cache)
{
    physaddr_t physical = (physaddr_t)frame_alloc(0, 0);

    if (physical == 0)
        return NULL;

    void *memory = phys_to_kern(physical);

    if (memory == NULL)
    {
        frame_free((void *)physical, 0);
        return NULL;
    }

    slab_t *slab = kmalloc(sizeof(slab_t));

    if (slab == NULL)
    {
        frame_free((void *)physical, 0);
        return NULL;
    }

    slab->physical = physical;
    slab->memory = memory;
    slab->capacity = cache->capacity;
    slab->inuse = 0;
    slab->state = SLAB_EMPTY;
    slab->free_objects = NULL;

    list_init(&slab->link);
    slab_build_free_objects(cache, slab);
    list_insert_after(&cache->empty_slabs, &slab->link);

    return slab;
}

/**
 * Selects an appropriate slab from the cache for allocation.
 * Returns a partial slab if available, otherwise an empty slab, or creates a new slab if necessary.
 */
static slab_t *slab_cache_select(slab_cache_t *cache)
{
    if (!list_empty(&cache->partial_slabs))
    {
        return list_container(cache->partial_slabs.next, slab_t, link);
    }

    if (!list_empty(&cache->empty_slabs))
    {
        return list_container(cache->empty_slabs.next, slab_t, link);
    }

    return slab_create(cache);
}

/**
 * Takes an object from the given slab.
 *
 * @param slab The slab to take the object from.
 * @return A pointer to the taken object, or NULL if the slab is empty.
 */
static void *slab_take_object(slab_t *slab)
{
    slab_free_object_t *object = slab->free_objects;

    if (object == NULL)
        return NULL;

    slab->free_objects = object->next;
    slab->inuse++;

    return object;
}

/**
 * Reclassifies a slab within the cache based on its current usage.
 *
 * @param cache The slab cache containing the slab.
 * @param slab The slab to be reclassified.
 */
static void slab_reclassify(slab_cache_t *cache, slab_t *slab)
{
    list_delete(&slab->link);

    if (slab->inuse == 0)
    {
        slab->state = SLAB_EMPTY;
        list_insert_after(&cache->empty_slabs, &slab->link);
    }
    else if (slab->inuse == slab->capacity)
    {
        slab->state = SLAB_FULL;
        list_insert_after(&cache->full_slabs, &slab->link);
    }
    else
    {
        slab->state = SLAB_PARTIAL;
        list_insert_after(&cache->partial_slabs, &slab->link);
    }

    KASSERT(slab->inuse <= slab->capacity);
}

void *slab_cache_alloc(slab_cache_t *cache)
{
    if (cache == NULL)
        return NULL;

    slab_t *slab = slab_cache_select(cache);

    if (slab == NULL)
        return NULL;

    KASSERT(slab->inuse < slab->capacity);
    KASSERT(slab->free_objects != NULL);

    void *object = slab_take_object(slab);

    if (object == NULL)
        return NULL;

    slab_reclassify(cache, slab);

    KASSERT(slab->inuse <= slab->capacity);
    KASSERT(slab_validate(cache, slab));

    return object;
}

/**
 * Finds the slab that owns the specified object within the given slab cache.
 *
 * @param cache The slab cache containing the slabs.
 * @param object The object to find the owning slab for.
 * @return A pointer to the slab containing the object, or NULL if not found.
 */
static slab_t *slab_cache_find_owner(slab_cache_t *cache, const void *object)
{
    list_t *lists[] = {&cache->partial_slabs, &cache->empty_slabs, &cache->full_slabs};

    for (size_t i = 0; i < sizeof(lists) / sizeof(lists[0]); i++)
    {
        slab_t *slab = slab_list_find(lists[i], cache, object);
        if (slab != NULL)
            return slab;
    }

    return NULL;
}

/**
 * Returns an object to the specified slab, marking it as free.
 *
 * @param slab The slab to return the object to.
 * @param object The object to be returned.
 */
static void slab_return_object(slab_t *slab, void *object)
{
    slab_free_object_t *free_object = (slab_free_object_t *)object;

    free_object->next = slab->free_objects;
    slab->free_objects = free_object;
    slab->inuse--;
}

int slab_cache_free(slab_cache_t *cache, void *object)
{
    if (cache == NULL || object == NULL)
        return -1;

    slab_t *slab = slab_cache_find_owner(cache, object);

    if (slab == NULL)
        return -1;

    if (slab->inuse == 0)
        return -1;

    if (slab_object_is_free(slab, object))
        return -1;

    slab_return_object(slab, object);
    slab_reclassify(cache, slab);

    KASSERT(slab_contains(cache, slab, object));
    KASSERT(slab_validate(cache, slab));

    return 0;
}