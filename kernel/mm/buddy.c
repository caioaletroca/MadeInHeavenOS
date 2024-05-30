#include <kprintf.h>
#include <mm/buddy.h>

/**
 * Calculates the first non-zero bit position starting from the left,
 * but counting from the right, minus 1.
 * Returns zero if val is zero.
 * Ex: _fnzb(0b0001110) = 3
 * 
 * @param value The value analyzed
*/
static inline unsigned int _fnzb(unsigned long value) {
    if(value == 0)
        return 0;
    return (sizeof(unsigned long) * 8) - __builtin_clzl(value) - 1;
}

frame_t buddy_alloc(const buddy_system_t *ctx, unsigned int order) {
    
}

int buddy_init(buddy_system_t *ctx, uint64_t frame_num, uint64_t frame_size) {
    uint64_t order_bit = _fnzb(frame_size);
    uint64_t order_max = _fnzb(frame_num);

    kprintf("Frames Size %u\n", frame_size);
    kprintf("Frames Num %u\n", frame_num);    

    // Initialize frames list
    ctx->frames = (frame_t *)kmalloc(frame_num * sizeof(frame_t));
    for(uint64_t i = 0; i < frame_num; i++) {
        list_init(&ctx->frames[i].link);
    }

    // Initialize free area
    ctx->order_bit = order_bit;
    ctx->order_max = order_max;
    ctx->free_area = (free_list_t *)kmalloc((ctx->order_max + 1) * sizeof(free_list_t));
    for(uint64_t i = 0; i < ctx->order_max; i++) {
        // Number of buddies of order i. Divide number of blocks by 2^(i + 1)
        // We need one bit per pair
        uint32_t frame_order_num = frame_num >> (i + 1);

        // Calculate the required number of unsigned long to hold the bitmap
        // Now, frame_order_num also holds the number of bits we need, ex: 70 bits
        // If this is a x86_64, an address has 8 bytes (64 bits)
        // By: (70 - 1) / (8 * 8) ; essencially we discover the round down number of unsigned long needed
        // Adds one to round up again, given 2 as result in the example
        frame_order_num = (frame_order_num - 1) / (8 * sizeof(unsigned long)) + 1;

        // Request memory for the map
        ctx->free_area[i].map = (unsigned long*)kmalloc(sizeof(unsigned long) * frame_order_num);

        // Clear memory
        memset(ctx->free_area[i].map, 0, sizeof(unsigned long) * frame_order_num);

        list_init(&ctx->free_area[i].list);
    }

    // Initialize the last (order max) entry with a null buddy
    list_init(&ctx->free_area[ctx->order_max - 1].list);
    ctx->free_area[ctx->order_max - 1].map = NULL;

    return 0;
}

void buddy_log(const buddy_system_t *ctx) {
    for(uint64_t i; i <= ctx->order_max; i++) {
        kprintf("order: %d", i);
        if(list_empty(&ctx->free_area[i].list)) {
            kprintf("  [ empty ]\n");
        }
        else {
            kprintf("  [ used ]\n");
        }
    }
}