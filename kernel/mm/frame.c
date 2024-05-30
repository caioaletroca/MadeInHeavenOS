#include <mm/frame.h>

int frame_zone_add(void *address, size_t size, size_t frame_size) {
    zone *z = (zone *)kmalloc(sizeof(zone));

    if(z == NULL) {
        return -1;
    }

    zone_init(z, address, size, frame_size);

    zone_log(z);

    return 0;
}