#include <addresses.h>
#include <kprintf.h>
#include <interrupts.h>
#include <mm.h>
#include "idt.h"
#include "driver/ps2.h"

void kmain(physaddr_t address) {
    struct multiboot_info *info = (struct multiboot_info*)phys_to_kern(address);

    kprintf("Alive!!!\n");

    interrupts_init();

    ps2_init();

    enable_interrupts();

    mmap_init(info);

    // kprintf("Phys Address: %p\n", address);
    // kprintf("Kernel Address: %p\n", phys_to_kern(address));
    // kprintf("Multiboot Total Size: %X\n", info->total_size);
    // kprintf("Reserved: %p\n", info->reserved);
    // kprintf("First Tag Type: %u\n", info->tags[0].type);

    // struct multiboot_tag* tags = (struct multiboot_tag*)info->tags;
    // for(; tags->type != 0; tags = (struct multiboot_tag*)ALIGNUP((uintptr_t)tags + tags->size, 8)) {
    //     kprintf("Tag Type: %u\n", tags->type);
    // }

    // Magic breakpoint
    __asm__ __volatile__ ("xchgw %bx, %bx");

    while(1) {
        __asm__ __volatile__ ("hlt");
    }
}