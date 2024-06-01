#include <mm/frame.h>

static zone_t *zone_list;

int frame_zone_add(void *address, size_t size, size_t frame_size) {
    zone_t *zone = (zone_t *)kmalloc(sizeof(zone_t));
    if(zone == NULL) {
        return -1;
    }

    if(zone_init(zone, address, size, frame_size, ZONE_NORMAL) != 0) {
        // TODO: Free kmalloc memory here
    }

    // Add newly created zone to the zone list
    zone->next = zone_list;
    zone_list = zone;

    return 0;
}

void frame_free(void *ptr, unsigned int order) {
    if(ptr == NULL) {
        return;
    }

    // Loop through all registered zones
    for(zone_t *zone = zone_list; zone != NULL; zone = zone->next) {
        // Sanity check for order
        if(order <= zone->buddy.order_max) {
            // Check if the address and order is in range of the current zone
            if(zone_contains(zone, (uintptr_t)ptr, (size_t)1 << (order + zone->buddy.order_bit))) {
                // Finally, free the memory
                zone_free(zone, ptr, order);
            }
        }
    }
}

void frame_log() {
    for(zone_t *zone = zone_list; zone != NULL; zone = zone->next) {
        zone_log(zone);
    }
}