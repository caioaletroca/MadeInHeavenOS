#include <mm/zone.h>

void *zone_alloc(const zone_t *ctx, int order) {
    buddy_alloc(&ctx->buddy, order);
}

void zone_free(const zone_t *ctx, const void *address, unsigned int order) {
    unsigned int index;
    frame_t *frame;

    // Calculate the index by subtracting the address from the base address
    // Then divide the result by the minimum frame size
    index = (unsigned int)(address - ctx->address) / (unsigned int)ctx->frame_size;
    frame = &ctx->buddy.frames[index];

    // Check this frame references
    if(frame->refs > 0) {
        frame->refs--;

        // If there is no more references to this frame, free it
        if(frame->refs == 0) {
            buddy_free(&ctx->buddy, frame, order);
        }
    }
}

int zone_init(zone_t *ctx, physaddr_t address, size_t size, size_t frame_size, int flags) {
    if(frame_size == 0) {
        return -1;
    }

    ctx->address = address;
    ctx->size = size;
    ctx->frame_size = frame_size;
    ctx->next = NULL;
    ctx->flags = flags;

    return buddy_init(&ctx->buddy, size / frame_size, frame_size);
}

void zone_log(const zone_t *ctx) {
    kprintf("Zone Start Address: 0x%p\n", ctx->address);
    kprintf("Zone End Address: 0x%p\n", ctx->address + ctx->size);
    buddy_log(&ctx->buddy, ctx->address);
}