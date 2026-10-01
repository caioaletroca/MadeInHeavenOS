#ifndef _MM_H_
#define _MM_H_

#include <stdint.h>
#include <boot_info.h>
#include <asm/memory.h>
#include <util.h>

/**
 * Low Level GFP Flags for Zone Allocation
 */

#define __GFP_DMA (1 << 0)
#define __GFP_HIGHMEM (1 << 1)

/**
 * Low Level GFP Flags for Allocation Behavior
 */

#define __GFP_WAIT (1 << 0)
#define __GFP_HIGH (1 << 1)
#define __GFP_IO (1 << 2)
#define __GFP_HIGHIO (1 << 3)
#define __GFP_FS (1 << 4)

#endif