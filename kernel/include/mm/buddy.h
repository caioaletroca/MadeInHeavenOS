#ifndef _BUDDY_H_
#define _BUDDY_H_

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <kmalloc.h>
#include <sys/list.h>

typedef struct frame {
    list_t link;
} frame_t;

/**
 * List of frames within the same order
*/
typedef struct free_list {
    // Free list node
    list_t list;

    // Bitmap used to keep track of the state of each couple of buddies
    unsigned long *map;
} free_list_t;

/**
 * A buddy system context
 * Allows for separate allocators for each memory zone
*/
typedef struct buddy_system {
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
    free_list_t *free_area;

    /**
     * 
    */
    frame_t *frames;
} buddy_system_t;

/**
 * Initialize the buddy memory allocator context to handle a chunk of memory.
 * 
 * TODO: Check for errors
 * 
 * @param ctx           Buddy system context pointer
 * @param frames_num    Number of frames to be handled
 * @param frame_size    Size of a single memory frame
 * @return              Returns zero on success and -1 as failure
*/
int buddy_init(buddy_system_t *ctx, uint64_t frame_num, uint64_t frame_size);

frame_t buddy_alloc(const buddy_system_t *ctx, unsigned int order);

/**
 * Logs information about the buddy system on stdout
 * 
 * @param ctx           Buddy system context pointer
*/
void buddy_log(const buddy_system_t *ctx);

#endif