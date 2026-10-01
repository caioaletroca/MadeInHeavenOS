#include <string.h>
#include <arch/irq.h>
#include <platform/platform.h>
#include <sched/scheduler.h>
#include <x86/idt.h>
#include <x86/isr.h>
#include <x86/vectors.h>

static isr_info_t isr_table[IDT_ENTRIES];
static irq_handler_t irq_handlers[VECTOR_IRQ_COUNT];

void isr_set_info(uint8_t vector, isr_info_t *info)
{
    memcpy(&isr_table[vector], info, sizeof(isr_info_t));
}

int irq_register(unsigned int irq, irq_handler_t handler)
{
    if (irq >= VECTOR_IRQ_COUNT || handler == NULL)
        return -1;

    irq_handlers[irq] = handler;
    platform_irq_enable(irq);
    return 0;
}

isr_context_t *isr_handler(isr_context_t *ctx)
{
    uint8_t vector = (uint8_t)(ctx->info >> 32) & 0xFF;

    if (vector >= VECTOR_IRQ_BASE && vector < VECTOR_IRQ_BASE + VECTOR_IRQ_COUNT)
    {
        // Hardware IRQ: dispatch by line number, then acknowledge it.
        // EOI must be sent before a possible switch: the next thread may
        // never return through this path.
        unsigned int irq = vector - VECTOR_IRQ_BASE;

        if (irq_handlers[irq] != NULL)
            irq_handlers[irq](irq);

        platform_irq_eoi(irq);
    }
    else
    {
        isr_info_t *info = &isr_table[vector];

        if (info->handler != NULL)
            info->handler(ctx);
    }

    // Never switch threads from a CPU exception.
    if (vector >= VECTOR_EXCEPTION_COUNT && scheduler_need_reschedule())
        return scheduler_on_interrupt(ctx);

    return ctx;
}
