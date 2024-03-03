#include "isr.h"

static isr_info_t isr_table[256];

void isr_set_info(uint8_t vector, isr_info_t *info) {
    memcpy(&isr_table[vector], info, sizeof(isr_info_t));
}

void isr_handler(isr_context *regs) {
    uint8_t int_no = (uint8_t)(regs->info >> 32) & 0xFF;
    isr_info_t *info = &isr_table[int_no];

    if(info->handler != NULL) {
        info->handler(regs);
    }
}