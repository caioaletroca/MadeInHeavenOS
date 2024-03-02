#include "interrupts.h"

/**
 * Initialize the interrupt sequence setup
*/
void interrupts_init() {
    // Create and fill the IDT
    idt_init();

    // Remap PIC interrupts
    pic_init();

    // Start interrupts
    idt_load();
}