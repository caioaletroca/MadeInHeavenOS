#ifndef _FRAME_H_
#define _FRAME_H_

#include <stddef.h>
#include <stdint.h>
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
int frame_zone_add(void *address, size_t size, size_t frame_size);

/**
 * Free a physical memory page
 * 
 * @param ptr           Memory physical address
 * @param order         Frame order
*/
void frame_free(void *ptr, unsigned int order);

/**
 * Logs frame information on stdout
*/
void frame_log();

#endif