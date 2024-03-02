#include <kprintf.h>
#include <interrupts.h>

void kmain() {
    interrupts_init();

    kprintf("%s Hello\n", "Test");

    __asm__ __volatile__ ("xchgw %bx, %bx");
    // __asm__ __volatile__ ("int $0x2");
}