#include "isr.h"
#include <driver/timer.h>

static volatile uint64_t ticks;

static void timer_irq_handler(isr_context_t *context)
{
    ticks++;
}

void timer_init(uint32_t frequency)
{
    isr_info_t timer_info = {
        .type = ISR_IRQ,
        .handler = timer_irq_handler,
    };

    isr_set_info(PIC_IRQ_VECTOR(PIC_IRQ_TIMER), &timer_info);
    pic_irq_enable(PIC_IRQ_TIMER);

    pit_set_frequency(frequency);
}

uint64_t timer_ticks(void)
{
    return ticks;
}