#include <platform/platform.h>
#include <driver/ps2.h>
#include "pic.h"

void platform_init(void)
{
    // Remap the PIC away from CPU exception vectors, all lines masked
    pic_init();

    // PS/2 controller and keyboard (registers IRQ 1)
    ps2_init();
}

void platform_irq_enable(unsigned int irq)
{
    pic_irq_enable((uint8_t)irq);
}

void platform_irq_eoi(unsigned int irq)
{
    pic_send_EOI((uint8_t)irq);
}
