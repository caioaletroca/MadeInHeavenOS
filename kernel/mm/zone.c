#include <mm/zone.h>

int zone_init(zone_t *ctx, physaddr_t address, size_t size, size_t frame_size) {
    if(frame_size == 0) {
        return -1;
    }

    ctx->address = address;
    ctx->size = size;
    ctx->frame_size = frame_size;
    ctx->next = NULL;

    return buddy_init(&ctx->buddy, size / frame_size, frame_size);
}

void zone_log(const zone_t *ctx) {
    kprintf("Zone Start Address: 0x%p\n", ctx->address);
    kprintf("Zone End Address: 0x%p\n", ctx->address + ctx->size);
    buddy_log(&ctx->buddy);
}