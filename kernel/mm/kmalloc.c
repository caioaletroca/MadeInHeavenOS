#include <kmalloc.h>
#include <addresses.h>
#include <mm/frame.h>
#include <mm/slab.h>
#include <mm/boot_alloc.h>
#include <panic.h>
#include <util.h>

#define KMALLOC_MIN_SHIFT 4  // 16 bytes
#define KMALLOC_MAX_SHIFT 10 // 1024 bytes
#define KMALLOC_MAX_SIZE (1UL << KMALLOC_MAX_SHIFT)
#define KMALLOC_ALIGNMENT 16
#define KMALLOC_CLASSES (KMALLOC_MAX_SHIFT - KMALLOC_MIN_SHIFT + 1)

static slab_cache_t kmalloc_caches[KMALLOC_CLASSES];
static const char *kmalloc_cache_names[KMALLOC_CLASSES] = {
    "kmalloc_16", "kmalloc_32", "kmalloc_64", "kmalloc_128", "kmalloc_256", "kmalloc_512", "kmalloc_1024"};

/**
 * Determines the appropriate kmalloc class for a given size.
 *
 * @param size The size of the memory block.
 * @return The index of the kmalloc class.
 */
static unsigned int kmalloc_class(size_t size)
{
    if (size <= (1UL << KMALLOC_MIN_SHIFT))
        return 0;

    return _fnzb(size - 1) + 1 - KMALLOC_MIN_SHIFT;
}

/**
 * Determines the appropriate page order for a given size.
 *
 * @param size The size of the memory block.
 * @return The order of the page allocation.
 */
static unsigned int kmalloc_page_order(size_t size)
{
    size_t pages = ALIGN_UP(size, PAGE_SIZE) / PAGE_SIZE;

    return _fnzb(pages) + ((pages & (pages - 1)) != 0);
}

void kmalloc_init(void)
{
    for (unsigned int i = 0; i < KMALLOC_CLASSES; i++)
    {
        size_t size = 1UL << (KMALLOC_MIN_SHIFT + i);

        if (slab_cache_init(&kmalloc_caches[i], kmalloc_cache_names[i], size, KMALLOC_ALIGNMENT) != 0)
            panic("kmalloc: cannot create %s", kmalloc_cache_names[i]);
    }

    // From here on, early allocations must not be used anymore
    boot_alloc_seal();
}

void *kmalloc(size_t size)
{
    if (size == 0)
        return NULL;

    if (size <= KMALLOC_MAX_SIZE)
        return slab_cache_alloc(&kmalloc_caches[kmalloc_class(size)]);

    unsigned int order = kmalloc_page_order(size);
    physaddr_t physical = frame_alloc(order, 0);

    if (physical == 0)
        return NULL;

    // Remember the order on the head frame so kfree() can find it
    frame_t *frame = frame_lookup(physical);
    frame->order = order;
    frame->slab = NULL;

    return phys_to_kern(physical);
}

void kfree(void *ptr)
{
    if (ptr == NULL)
        return;

    if (slab_free(ptr) == 0)
        return;

    // Not a slab object: must be the head of a page allocation
    physaddr_t physical = kern_to_phys(ptr);
    frame_t *frame = frame_lookup(physical);

    if (frame == NULL || (physical & (PAGE_SIZE - 1)) != 0)
        panic("kfree: invalid pointer %p", ptr);

    frame_free(physical, frame->order);
}