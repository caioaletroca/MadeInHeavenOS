#include <kprintf.h>
#include <interrupts.h>
#include "idt.h"
#include "driver/ps2.h"

void kmain() {
    kprintf("Alive!!!\n");

    interrupts_init();

    ps2_init();

    enable_interrupts();

    __asm__ __volatile__ ("xchgw %bx, %bx");

    // __asm__ __volatile__ ("int $2");
    while(1) {
        __asm__ __volatile__ ("hlt");
    }
}