#ifndef _MULTIBOOT_2_H_
#define _MULTIBOOT_2_H_

/**
 * Information about this file can be found following the
 * Multiboot2 specification at
 * https://www.gnu.org/software/grub/manual/multiboot2/multiboot.html#Boot-information-format
*/

typedef unsigned char           multiboot_uint8_t;
typedef unsigned short          multiboot_uint16_t;
typedef unsigned int            multiboot_uint32_t;
typedef unsigned long long      multiboot_uint64_t;

#define MULTIBOOT_MEMORY_AVAILABLE           1
#define MULTIBOOT_MEMORY_RESERVED            2
#define MULTIBOOT_MEMORY_ACPI_RECLAIMABLE    3
#define MULTIBOOT_MEMORY PRESERVED           4
#define MULTIBOOT_MEMORY_BADRAM              5

struct multiboot_mmap_entry {
    multiboot_uint64_t base_address;
    multiboot_uint64_t length;
    multiboot_uint32_t type;
    multiboot_uint32_t reserved;
};

struct multiboot_tag {
    multiboot_uint32_t type;
    multiboot_uint32_t size;
};

struct multiboot_tag_mmap {
    multiboot_uint32_t type;
    multiboot_uint32_t size;
    multiboot_uint32_t entry_size;
    multiboot_uint32_t entry_version;
    struct multiboot_mmap_entry entries[];
};

struct multiboot_info {
	multiboot_uint32_t total_size;
	multiboot_uint32_t reserved;
	struct multiboot_tag tags[];
};

#endif