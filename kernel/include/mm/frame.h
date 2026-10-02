#ifndef _FRAME_H_
#define _FRAME_H_

#include <stddef.h>
#include <stdint.h>
#include <addresses.h>
#include <mm/zone.h>
#include <kmalloc.h>

/**
 * Adds a memory zone to the frame allocator
 *
 * @param address       Zone frame address
 * @param size          Size of the zone
 * @param frame_size    Size of frames within the zone
 * @param flags         Frame zone flags
 * @return              Returns 0 on success, -1 on error
 */
int frame_zone_add(physaddr_t address, size_t size, size_t frame_size);

/**
 * Allocate a physical memory page
 *
 * @param order         Frame order
 * @param flags         Allocation flags
 * @return              Memory physical address
 */
physaddr_t frame_alloc(unsigned int order, unsigned int flags);

/**
 * Free a physical memory page
 *
 * @param address       Memory physical address
 * @param order         Frame order
 */
void frame_free(physaddr_t address, unsigned int order);

/**
 * Look up the frame structure corresponding to a physical address
 *
 * @param address       Memory physical address
 * @return              Pointer to the frame structure
 */
frame_t *frame_lookup(physaddr_t address);

/**
 * Logs frame information on stdout
 */
void frame_log();

#endif