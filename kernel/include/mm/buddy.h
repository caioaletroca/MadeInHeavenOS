#ifndef _BUDDY_H_
#define _BUDDY_H_

#include <stddef.h>
#include <stdint.h>
#include <sys/list.h>

struct frame {
    struct list link;
};

struct free_list {

};

/**
 * A buddy system context
 * Allows for separate allocators for each memory zone
*/
struct buddy_system {
    /**
     * 
    */
    uint64_t order_bit;

    /**
     * Max order for the buddy allocator
    */
    uint64_t order_max;

    /**
     * Free area for the memory zone
    */
    struct free_list *free_area;

    /**
     * 
    */
    struct frame *frames;
};

int buddy_init(struct buddy_system *b, uint64_t frame_num, uint64_t frame_size);

void buddy_log(const struct buddy_system *b);

#endif