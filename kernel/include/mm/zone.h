#ifndef _ZONE_H_
#define _ZONE_H_

#include <stddef.h>
#include <addresses.h>
#include <mm/buddy.h>

typedef struct _zone {
    // Zone physical address
    physaddr_t address;

    // Zone size
    size_t size;

    // Size of a single frame
    size_t frame_size;

    // Next zone pointer
    struct zone *next;

    // Boddy system for the zone
    struct buddy_system buddy;
} zone;

int zone_init(zone *z, physaddr_t address, size_t size, size_t frame_size);

/**
 * Logs zone information on stdout
 * 
 * @param z Zone structure pointer
*/
void zone_log(const zone *z);

#endif