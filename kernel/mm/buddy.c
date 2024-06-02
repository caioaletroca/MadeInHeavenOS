#include <kprintf.h>
#include <mm/buddy.h>

/**
 * Toggles the bit for a couple of buddies
 * It uses only one bit for a couple, and returns the modified bit value
 * 
 * @param ctx           Buddy system context pointer
 * @param block_index   Index of the block to be toggled
 * @param order         Order of the block
 * @return              The modified value
*/
static int toggle_bit(const buddy_system_t *ctx, unsigned int block_index, unsigned int order)
{
    unsigned long *word, bit;
    unsigned int bit_index = block_index >> (order + 1);

    // From the right free area index given by the order
    // Find inside the map, the unsigned long that corresponds the block,
    // This is done by dividing the bit_index by the size of each word
    // Ex: in 64 bits, if bit_index = 68, size of unsigned long is 8
    // Then 68 / 16 = 4th word.
    word = &ctx->free_area[order].map[bit_index / (8 * sizeof(unsigned long))];

    // Now creates a new word with the right index toggle as 1
    // This is done by module operation with the size of each word
    // Ex: in 64 bits, if bit_index = 68, size of unsigned long is 8
    // Then 68 % 16 = 4th bit
    bit = 1UL << (bit_index % (8 * sizeof(unsigned long)));

    // Exclusive OR (XOR) operation on the word and the bit with the right bit set.
    *word ^= bit;
    
    // Return the right word zero-ed with only the state of the right bit
    // If the bit was set, then this should result into a non-zero value
    // In other way, return the current value
    return *word & bit;
}

/**
 * ATENÇÃO
 * NO BITMAP, 1 SIGNIFICA LIVRE, E 0 SIGNIFICA SPLIT
 * PORQUE A MEMÓRIA COMEÇA TOTALMENTE FRAGMENTADA, E LIBERAMOS LOGO NA SEQUÊNCIA PARA
 * TORNAR UM BLOCÃO SÓ
*/

frame_t *buddy_alloc(const buddy_system_t *ctx, unsigned int order) {
    frame_t *frame = NULL;
    unsigned int current_order = 0;
    unsigned int left_index;
    unsigned int right_index;

    // Loop through orders starting from the order param going up
    for(current_order = order; current_order <= ctx->order_max; current_order++) {
        // If the list is not empty, we found a new block to allocate
        if(!list_empty(&ctx->free_area[current_order].list)) {
            // Get the container frame of this link/list
            frame = list_container(ctx->free_area[current_order].list.next,
                                 frame_t, link);

            // Calculate the frame index by subtracting the base address
            // of the first frame from the current frame address
            left_index = frame - ctx->frames;

            break;
        }
    }

    // There's no available frame
    if(current_order > ctx->order_max) {
        return NULL;
    }

    // Toggle bit for the newly allocated frame
    // Order max doesn't have buddies
    if(current_order != ctx->order_max) {
        toggle_bit(ctx, left_index, current_order);
    }

    // Split free area if needed
    while(current_order > order) {
        current_order--;
        right_index = left_index + (1 << current_order);

        list_insert_before(&ctx->free_area[current_order].list, &ctx->frames[right_index].link);
        toggle_bit(ctx, right_index, current_order);
    }

    return frame;
}

void buddy_free(const buddy_system_t *ctx, const frame_t *frame, unsigned int order) {
    unsigned int frame_index;
    unsigned int buddy_index;

    // Block index will be the address distance between the frame and the first frame
    frame_index = frame - ctx->frames;
    
    // Loop through all orders from the current order to max
    while(order != ctx->order_max) {
        // Calculate the buddy index by XOR the frame_index
        // with 2^(order)
        buddy_index = frame_index ^ (1 << order);

        // Toggle the couple bit on the bitmap
        // If the new value is 1, that means the buddy is also free
        // So continue to merge blocks
        if (toggle_bit(ctx, buddy_index, order) != 0)
            break;

        // Remove the buddy from its free list
        list_delete(&ctx->frames[buddy_index].link);

        // Always keep track of the left side index
        if(buddy_index < frame_index) {
            frame_index = buddy_index;
        }

        order++;
    }

    // Insert the block at the end of the proper list
    list_insert_before(&ctx->free_area[order].list, &ctx->frames[frame_index].link);
}

int buddy_init(buddy_system_t *ctx, uint64_t frame_num, uint64_t frame_size) {
    uint64_t order_bit = _fnzb(frame_size);
    uint64_t order_max = _fnzb(frame_num);

    // Initialize frames list
    ctx->frames = (frame_t *)kmalloc(frame_num * sizeof(frame_t));
    for(uint64_t i = 0; i < frame_num; i++) {
        list_init(&ctx->frames[i].link);
        ctx->frames[i].refs = 1;
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
    list_init(&ctx->free_area[ctx->order_max].list);
    ctx->free_area[ctx->order_max].map = NULL;

    return 0;
}

void buddy_log(const buddy_system_t *ctx, void *address) {
    size_t free_memory = 0;

    // Loop through all free area
    for(unsigned int i = 0; i <= ctx->order_max; i++) {
        kprintf("order: %d", i);
        if(list_empty(&ctx->free_area[i].list)) {
            kprintf("  [ empty ]\n");
        }
        else {
            kprintf("\n");

            // Loop through all free area lists
            for(list_t *frame_link = ctx->free_area[i].list.next;
                frame_link != &ctx->free_area[i].list;
                frame_link = frame_link->next) {
                
                // Get the frame owner of the link
                frame_t *frame = list_container(frame_link, frame_t, link);

                // Calculate the frame index by subtracting the base address
                // of the first frame from the current frame address
                unsigned int frame_index = frame - ctx->frames;

                // Calculate the frame address using the zone base address
                void *frame_ptr = address + (frame_index << ctx->order_bit);

                kprintf("  [ 0x%p : 0x%p ]\n", frame_ptr, frame_ptr + (1 << (ctx->order_bit+i)));
                
                // Sums free memory
                free_memory += (1 << (ctx->order_bit + i));
            }
        }
    }
    kprintf("free memory: %u\n", free_memory);
}