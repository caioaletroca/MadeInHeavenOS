#ifndef _MMAP_H_
#define _MMAP_H_

#include <mm/frame.h>
#include <util.h>

/**
 * MMap memory region structure
*/
typedef struct mmap_region {
    // Memory base address
    uintptr_t base;

    // Memory size
    size_t size;
} mmap_region_t;

/**
 * MMap memory type structure
*/
typedef struct mmap_type {
    // List of memory regions
    mmap_region_t *regions;

    // Length of the list
    uint32_t length;
} mmap_type_t;

/**
 * MMap structure
*/
typedef struct mmap {
    // List of available memory
    mmap_type_t available;

    // List of reserved memory
    mmap_type_t reserved;
} mmap_t;

/**
 * Swaps the position of two memory regions
 * 
 * @param x             First memory region pointer
 * @param y             Second memory region pointer
*/
static void mmap_swap_region(mmap_region_t *x, mmap_region_t *y);

/**
 * Sort regions by base address
 * Implements bubble sort
 * 
 * @param type          Memory type context pointer
*/
static void mmap_sort_region(mmap_type_t *type);

/**
 * Merges the regions close together into bigger blocks of memory
 * 
 * @param type          Memory type context pointer
*/
static void mmap_merge_region(mmap_type_t *type);

/**
 * Splits the memory regions into multiples of 2^(n)
 * 
 * @param type          Memory type context pointer
 * @param frame_size    The size of each frame
*/
static void mmap_split_region(mmap_type_t *type, size_t frame_size);

/**
 * Removes a region from the array
 * 
 * @param type          Memory type pointer
 * @param i             The index of the region to be removed
*/
static void mmap_remove_region(mmap_type_t *type, size_t i);

/**
 * Adds a new region to the array
 * 
 * @param type          Memory type pointer
 * @param address       New region base address
 * @param size          New region size
*/
static void mmap_insert_region(mmap_type_t *type, uintptr_t address, size_t size);

/**
 * Registers all regions to allocator
 * 
 * @param type          Memory type context pointer
 * @param frame_size    The size of each frame
*/
static void mmap_register_region(mmap_type_t *type, size_t frame_size);

/**
 * Parse through multiboot memory information
 * and populate a internal memory layout tagging as available
 * and reserved.
 * 
 * @param info          Multiboot2 information structure pointer
*/
void mmap_init(struct multiboot_info *info);

/**
 * Logs Memory Map information to stdout
 * 
 * @param ctx       Memory map context pointer
*/
static void mmap_log(mmap_t *ctx);

#endif