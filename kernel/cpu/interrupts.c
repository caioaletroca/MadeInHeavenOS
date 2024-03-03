#include "interrupts.h"

void interrupts_init() {
    // Create and fill the IDT
    idt_init();

    // Remap PIC interrupts
    pic_init();

    // Initialize CPU exceptions ISR
    exception_init();

    // Start interrupts
    idt_load();
}