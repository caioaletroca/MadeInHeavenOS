#ifndef _BOOT_INFO_H_
#define _BOOT_INFO_H_

#define BOOT_MEMORY_MAX_REGIONS 64
#define BOOT_CMDLINE_MAX 256

#include <addresses.h>

/**
 * @brief Physical memory region types, independent of the boot protocol.
 */
typedef enum boot_memory_type
{
    BOOT_MEMORY_AVAILABLE,
    BOOT_MEMORY_RESERVED,
    BOOT_MEMORY_ACPI_RECLAIMABLE,
    BOOT_MEMORY_ACPI_NVS,
    BOOT_MEMORY_BAD,
} boot_memory_type_t;

/**
 * @brief Describes a physical memory region.
 */
typedef struct boot_memory_region
{
    physaddr_t base;
    uint64_t length;
    boot_memory_type_t type;
} boot_memory_region_t;

typedef struct boot_info
{
    boot_memory_region_t memory_regions[BOOT_MEMORY_MAX_REGIONS];
    size_t memory_region_count;
    char cmdline[BOOT_CMDLINE_MAX];
} boot_info_t;

#endif /* _BOOT_INFO_H_ */