#ifndef _ZONE_H_
#define _ZONE_H_

#include <stddef.h>
#include <stdbool.h>
#include <addresses.h>
#include <mm/buddy.h>

/**
 * Memory in this zone is used for devices that require DMA (Direct Memory Access)
 * and can only address the lower part of the memory (e.g., the first 16 MB).
*/
#define ZONE_DMA        0

/**
 * Used for devices that can address the lower 4 GB of memory,
 * mainly relevant for 32-bit devices on a 64-bit system.
*/
#define ZONE_DMA32      1

/**
 * This is the main memory zone from which kernel and user-space applications typically allocate memory.
 * It covers the normal range of memory that is directly addressable by the kernel.
*/
#define ZONE_NORMAL     2

/**
 * Used in 32-bit systems for memory that is not permanently mapped into
 * the kernel address space (not used in 64-bit systems).
*/
#define ZONE_HIGHMEM    3

/**
 * Used for memory that can be moved around to reduce fragmentation
 * and make more contiguous memory available for ZONE_NORMAL and ZONE_DMA.
*/
#define ZONE_MOVABLE    4

/**
 * Memory zone descriptor with a given base address.
 * Has it own buddy allocator system.
*/
typedef struct zone {
    // Zone physical address
    physaddr_t address;

    // Zone size
    size_t size;

    // Size of a single frame
    size_t frame_size;

    // Type of zone
    unsigned int flags;

    // Next zone pointer
    struct zone *next;

    // Boddy system for the zone
    struct buddy_system buddy;
} zone_t;

/**
 * Initialize a zone descriptor structure
 * 
 * @param ctx           Zone descriptor pointer
 * @param address       Zone physical address
 * @param size          Zone size
 * @param frame_size    Size of the frame within the zone
 * @param flags         Zone flags
 * @return              Returns 0 as success, and -1 on error
*/
int zone_init(zone_t *ctx, physaddr_t address, size_t size, size_t frame_size, int flags);

void *zone_alloc(const zone_t *ctx, int order);

/**
 * Free a memory segment from the zone.
 * 
 * @param ctx           Zone descriptor pointer
 * @param address       Pointer to a memory chunk
 * @param order         Frame order
*/
void zone_free(const zone_t *ctx, const void *address, unsigned int order);

/**
 * Checks if a memory area is contained in a zone
 * 
 * @param ctx           Zone descriptor pointer
 * @param address       A memory address
 * @param size          The memory size
*/
bool zone_contains(const zone_t *ctx, uintptr_t address, size_t size);

/**
 * Logs zone information on stdout
 * 
 * @param z             Zone structure pointer
*/
void zone_log(const zone_t *ctx);

#endif