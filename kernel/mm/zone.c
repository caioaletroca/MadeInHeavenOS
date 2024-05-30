#include <mm/zone.h>

int zone_init(zone *z, physaddr_t address, size_t size, size_t frame_size) {
    if(frame_size == 0) {
        return -1;
    }

    z->address = address;
    z->size = size;
    z->frame_size = frame_size;
    z->next = NULL;

    return buddy_init(&z->buddy, size / frame_size, frame_size);
}

void zone_log(const zone *z) {
    kprintf("Zone Start Address: 0x%p\n", z->address);
    kprintf("Zone End Address: 0x%p\n", z->address + z->size);
    buddy_log(&z->buddy);
}