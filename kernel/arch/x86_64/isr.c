#include "isr.h"

static isr_info_t isr_table[256];

void isr_set_info(uint8_t vector, isr_info_t *info)
{
    memcpy(&isr_table[vector], info, sizeof(isr_info_t));
}

void isr_handler(isr_context_t *regs)
{
    uint8_t int_no = (uint8_t)(regs->info >> 32) & 0xFF;
    isr_info_t *info = &isr_table[int_no];

    if (info->handler != NULL)
    {
        info->handler(regs);
    }

    // Handle IRQs by sending EOI to the PIC if necessary
    if (info->type == ISR_IRQ && int_no >= PIC1_VECTOR_OFFSET && int_no < PIC2_VECTOR_OFFSET + 8)
    {
        pic_send_EOI(int_no - PIC1_VECTOR_OFFSET);
    }
}