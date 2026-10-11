#ifndef _MMAP_H_
#define _MMAP_H_

#include <mm/frame.h>
#include <util.h>

/**
 * MMap memory region structure
 */
typedef struct mmap_region
{
    // Memory base address
    uintptr_t base;

    // Memory size
    size_t size;
} mmap_region_t;

/**
 * MMap memory type structure
 */
typedef struct mmap_type
{
    // List of memory regions
    mmap_region_t *regions;

    // Length of the list
    uint32_t length;
} mmap_type_t;

/**
 * MMap structure
 */
typedef struct mmap
{
    // List of available memory
    mmap_type_t available;

    // List of reserved memory
    mmap_type_t reserved;
} mmap_t;

/**
 * Parse through multiboot memory information
 * and populate a internal memory layout tagging as available
 * and reserved.
 *
 * @param info          Multiboot2 information structure pointer
 */
void mmap_init(const boot_info_t *info);

/**
 * Logs Memory Map information to stdout
 *
 * @param ctx       Memory map context pointer
 */
static void mmap_log(mmap_t *ctx);

#endif