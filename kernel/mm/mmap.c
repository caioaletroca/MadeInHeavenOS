#include <mm.h>
#include <kprintf.h>
#include <mm/mmap.h>

extern const uintptr_t _kernel_physical_end;

#define MMAP_MAX_REGIONS 128

mmap_region_t available[MMAP_MAX_REGIONS];
mmap_region_t reserved[MMAP_MAX_REGIONS];

static mmap_t memory_map = {
    .available = { .length = 0, .regions = available },
    .reserved = { .length = 0, .regions = reserved }
};

static void mmap_free(uintptr_t start, uintptr_t end) {
    uintptr_t current = start;

    kprintf("Start Address: %p\n", start);
    kprintf("End Address: %p\n", end);

    while(current < end) {
        frame_free(current, 0);
        current += PAGE_SIZE;
    }
}

static void mmap_swap_region(mmap_region_t *x, mmap_region_t *y) {
    mmap_region_t temp = *x;
    *x = *y;
    *y = temp;
}

static void mmap_sort_region(mmap_type_t *type) {
    bool swapped = false;

    // Loops through all regions
    for(size_t i = 0; i < type->length - 1; i++) {
        swapped = false;

        // Loops again
        for(size_t j = 0; j < type->length - i - 1; j++) {
            // Check if the current base address is higher than the next one
            // Swap regions if needed
            if(type->regions[j].base > type->regions[j + 1].base) {
                mmap_swap_region(&type->regions[j], &type->regions[j + 1]);
                swapped = true;
            }

        }

        // Breaks loop earlier if no swaps happened
        if(swapped == false) {
            break;
        }
    }
}

static void mmap_merge_region(mmap_type_t *type) {
    size_t i = type->length - 1;

    // Loops in descending order through all regions
    while(i > 0) {
        uintptr_t current_address = type->regions[i].base;
        size_t current_size = type->regions[i].size;

        // If the region before end address is equal to the current address
        // That means we can merge those two regions
        if(type->regions[i - 1].base + type->regions[i - 1].size == current_address) {
            // Merge the sizes
            type->regions[i - 1].size += current_size;

            // Remove the current and now extra region
            mmap_remove_region(type, i);
        }

        i--;
    }
}

static void mmap_split_region(mmap_type_t *type, size_t frame_size) {
    // Loop through all regions
    for(size_t i = 0; i < type->length; i++) {
        uintptr_t current_address = type->regions[i].base;
        size_t current_size = type->regions[i].size;

        // Calculate the max order of this size
        size_t frame_num = current_size / frame_size;
        unsigned int order_max = _fnzb(frame_num);

        // The new size needs to be divisible by 2^(order_max)
        size_t new_size = (1 << order_max) * frame_size;

        // Update the current region
        type->regions[i].size = new_size;

        // If this new split is around the 4th order,
        // That means the rest of space is smaller than 4 frame sizes,
        // so ignore this extra space, is now unmapped memory
        if(order_max >= 4) {
            // Insert the rest of the memory available into a new region
            // Offsetting the address by the new size, and using the rest of the size
            mmap_insert_region(type, current_address + new_size, current_size - new_size);
        }
    }
}

static void mmap_remove_region(mmap_type_t *type, size_t i) {
    memmove(type->regions + i, type->regions + i + 1, (type->length - i - 1) * sizeof(mmap_region_t));
    type->length--;
}

static void mmap_insert_region(mmap_type_t *type, uintptr_t address, size_t size) {
    if(size == 0) {
        return;
    }

    // Adds new region to the end of array
    const mmap_region_t new_region = {
        .base = address,
        .size = size
    };
    type->regions[type->length++] = new_region;
}

static void mmap_register_region(mmap_type_t *type, size_t frame_size) {
    for(size_t i = 0; i < type->length; i++) {
        frame_zone_add(type->regions[i].base, type->regions[i].size, frame_size);
    }
}

void mmap_init(struct multiboot_info *info) {
    struct multiboot_tag *first_tag = (struct multiboot_tag *)info->tags;
    
    uintptr_t kern_end = (uintptr_t)&_kernel_physical_end + KERNEL_HEAP_SIZE;
    kprintf("Kernel End: %p\n", kern_end);
    
    // Search if there is any Memory Map Tag, meaning type == 6
    struct multiboot_tag *tag = first_tag;
    for(
        ;
        tag->type != MULTIBOOT_TAG_END;
        tag = (struct multiboot_tag *)ALIGN_UP((uintptr_t)tag + tag->size, 8)) {

        // If this is the tag about Memory Map
        if(tag->type == MULTIBOOT_TAG_MMAP) {
            struct multiboot_tag_mmap *mmap_tag = (struct multiboot_tag_mmap *)tag;

            // Interate over all entries inside the Memory Map Tag
            struct multiboot_mmap_entry *first_entry = (struct multiboot_mmap_entry *)mmap_tag->entries;
            for(
                struct multiboot_mmap_entry *entry = first_entry;
                (uint8_t *) entry < (uint8_t *)mmap_tag + mmap_tag->size;
                entry = (struct multiboot_mmap_entry *)((uintptr_t)entry + mmap_tag->entry_size)
            ) {
                // If this entry is type as available
                if(entry->type == MULTIBOOT_MEMORY_AVAILABLE) {    
                    // Check if the current entry is before the Kernel
                    if(entry->base_address + entry->length < kern_end) {
                        mmap_insert_region(&memory_map.reserved, entry->base_address, entry->length);
                    }
                    // Now, if the zone overlaps with the kernel, relocate the base address
                    // to be after the Kernel
                    else if(entry->base_address < kern_end) {
                        uintptr_t base = ALIGN_UP(kern_end, PAGE_SIZE);
                        size_t length = ALIGN_DOWN(entry->length - (kern_end - entry->base_address), PAGE_SIZE);

                        mmap_insert_region(&memory_map.available, base, length);
                    }
                    // The entry is available
                    else {
                        uintptr_t base = ALIGN_UP(entry->base_address, PAGE_SIZE);
                        size_t length = ALIGN_DOWN(entry->length, PAGE_SIZE);

                        mmap_insert_region(&memory_map.available, base, length);
                    }
                }
            }
        }
    }

    mmap_sort_region(&memory_map.available);
    mmap_merge_region(&memory_map.available);
    mmap_split_region(&memory_map.available, PAGE_SIZE);
    mmap_sort_region(&memory_map.available);

    mmap_register_region(&memory_map.available, PAGE_SIZE);
}

static void mmap_log(mmap_t *ctx) {
    kprintf("- Memory Map -\n");

    kprintf("Reserved:    [ %u ]\n", ctx->reserved.length);
    for(size_t i = 0; i < ctx->reserved.length; i++) {
        mmap_region_t *region = &ctx->reserved.regions[i];
        kprintf("    [ 0x%p : 0x%p ] %u bytes\n", region->base, region->base + region->size - 1, region->size);
    }

    kprintf("Available:   [ %u ]\n", ctx->available.length);
    for(size_t i = 0; i < ctx->available.length; i++) {
        mmap_region_t *region = &ctx->available.regions[i];
        kprintf("    [ 0x%p : 0x%p ] %u bytes\n", region->base, region->base + region->size - 1, region->size);
    }
}