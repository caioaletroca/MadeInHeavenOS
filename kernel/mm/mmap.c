#include <mm.h>
#include <kprintf.h>
#include <mm/frame.h>

extern const uintptr_t _kernel_start;
extern const uintptr_t _kernel_physical_end;

void mmap_init(struct multiboot_info *info) {
    struct multiboot_tag *first_tag = (struct multiboot_tag *)info->tags;
    uintptr_t kern_start = (uintptr_t)&_kernel_start;
    uintptr_t kern_end = (uintptr_t)&_kernel_physical_end + KERNEL_HEAP_SIZE;
    kprintf("Kernel Start: %p\n", kern_start);
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
            kprintf("Memory map found size: %u\n", mmap_tag->size);

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
                        // Do nothing
                    }
                    // Now, if the zone overlaps with the kernel, relocate the base address
                    // to be after the Kernel
                    else if(entry->base_address < kern_end) {
                        uintptr_t new_base = kern_end;
                        size_t new_length = entry->length - (kern_end - entry->base_address);

                        kprintf("New Base Address: %p\n", new_base);
                        kprintf("New End Address: %p\n", new_base + new_length);

                        frame_zone_add(new_base, new_length, 0x1000);
                    }
                    // The entry is available
                    else {
                        frame_zone_add(entry->base_address, entry->length, 0x1000);
                    }
                }
            }
        }
    }
}