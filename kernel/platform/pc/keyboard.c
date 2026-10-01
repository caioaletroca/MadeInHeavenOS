#include <arch/irq.h>
#include <driver/keyboard.h>
#include <driver/ps2.h>
#include <kprintf.h>
#include <x86/io.h>
#include "pic.h"

static void ps2_keyboard_irq_handler(unsigned int irq)
{
    (void)irq;

    while ((inb(PS2_STATUS) & 1) == 0)
        ;
    uint8_t scancode = inb(PS2_DATA);

    kprintf("SCANCODE SET 2: 0x%X\n", scancode);
}

void ps2_keyboard_init(void)
{
    irq_register(PIC_IRQ_KEYBOARD, ps2_keyboard_irq_handler);
}
