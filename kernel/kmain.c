#include <kprintf.h>
#include <interrupts.h>

void kmain() {
    interrupts_init();

    kprintf("Alive!!!\n");

    __asm__ __volatile__ ("xchgw %bx, %bx");

    while(1) {
        __asm__ __volatile__ ("hlt");
    }
    // __asm__ __volatile__ ("int $33");
}