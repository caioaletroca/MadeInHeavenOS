#include "isr.h"
#include <kprintf.h>

void isr_handler(isr_context isr_reg) {
    kprintf("Interrupt!!!");
}