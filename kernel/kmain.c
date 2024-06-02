#include <addresses.h>
#include <kprintf.h>
#include <interrupts.h>
#include <mm.h>
#include "idt.h"
#include "driver/ps2.h"

void kmain(physaddr_t address) {
    struct multiboot_info *info = (struct multiboot_info*)phys_to_kern(address);

    kprintf("MiHOS\n");

    interrupts_init();

    ps2_init();

    enable_interrupts();

    mmap_init(info);

    // Magic breakpoint
    __asm__ __volatile__ ("xchgw %bx, %bx");

    while(1) {
        __asm__ __volatile__ ("hlt");
    }
}