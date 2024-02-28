#include <kprintf.h>
#include <idt.h>

void kmain() {
    idt_init();

    kprintf("%s Hello\n", "Test");

    __asm__ __volatile__ ("xchgw %bx, %bx");
    __asm__ __volatile__ ("int $0x2");
}