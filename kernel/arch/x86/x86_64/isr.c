#include "isr.h"
#include <driver/timer.h>

static isr_info_t isr_table[256];

void isr_set_info(uint8_t vector, isr_info_t *info)
{
    memcpy(&isr_table[vector], info, sizeof(isr_info_t));
}

isr_context_t *isr_handler(isr_context_t *ctx)
{
    uint8_t int_no = (uint8_t)(ctx->info >> 32) & 0xFF;
    isr_info_t *info = &isr_table[int_no];

    if (info->handler != NULL)
        info->handler(ctx);

    // Handle IRQs by sending EOI to the PIC if necessary
    if (info->type == ISR_IRQ && int_no >= PIC1_VECTOR_OFFSET && int_no < PIC2_VECTOR_OFFSET + 8)
        pic_send_EOI(int_no - PIC1_VECTOR_OFFSET);

    // Check if the scheduler needs to reschedule after handling the interrupt
    if (info->type != ISR_EXCEPTION && scheduler_need_reschedule())
    {
        return scheduler_on_interrupt(ctx);
    }

    return ctx;
}