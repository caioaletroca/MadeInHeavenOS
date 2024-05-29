#include <kprintf.h>
#include <mm/buddy.h>

int buddy_init(struct buddy_system *b, uint64_t frame_num, uint64_t frame_size) {
    uint64_t order_bit = __builtin_clz(frame_size);
    uint64_t order_max = __builtin_clz(frame_num);

    b->order_bit = order_bit;
    b->order_max = order_max;

    return 0;
}

/**
 * Logs information about the buddy system on stdout
 * 
 * @param b Buddy system structure pointer
*/
void buddy_log(const struct buddy_system *b) {
    kprintf("----------------------------------------\n");
    kprintf("\tBuddy Allocator Log\n");
    kprintf("----------------------------------------\n");

    kprintf("order max: %d\n", b->order_max);

    for(uint64_t i; i <= b->order_max; i++) {
        // kprintf("order: %d\n", i);
    }
}