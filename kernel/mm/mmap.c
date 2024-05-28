#include <mm.h>
#include <kprintf.h>

extern const uintptr_t _kernel_start;
extern const uintptr_t _kernel_physical_end;

void mmap_init(struct multiboot_info *info) {
    struct multiboot_tag *first_tag = (struct multiboot_tag *)info->tags;
    uintptr_t kern_start = (uintptr_t)&_kernel_start;
    uintptr_t kern_end = (uintptr_t)&_kernel_physical_end;
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
                    
                }

                kprintf("Type: %u\n", entry->type);
                kprintf("Found Available Base Address: %p\n", entry->base_address);
                // kprintf("Length: %u\n", entry->length);
                kprintf("End Address: %p\n", entry->base_address + entry->length);
            }
        }
    }
}