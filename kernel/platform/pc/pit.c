#include <arch/irq.h>
#include <driver/timer.h>
#include <platform/platform.h>
#include <x86/io.h>
#include "pic.h"

// Programmable Interval Timer (PIT) driver for the PC platform
#define PIT_COMMAND 0x43
#define PIT_CHANNEL0 0x40

// PIT operating modes
#define PIT_MODE0 0x30
#define PIT_MODE1 0x30
#define PIT_MODE2 0x34
#define PIT_MODE3 0x36

// PIT base frequency in Hz
#define PIT_FREQUENCY 1193182

static void pit_set_frequency(uint32_t frequency)
{
    uint16_t divisor = PIT_FREQUENCY / frequency;

    // Set PIT to mode 3 (square wave) with the calculated frequency
    outb(PIT_COMMAND, PIT_MODE3);

    // Send the low and high bytes of the divisor to the PIT channel 0
    outb(PIT_CHANNEL0, (uint8_t)(divisor & 0xFF));
    outb(PIT_CHANNEL0, (uint8_t)((divisor >> 8) & 0xFF));
}

static void pit_irq_handler(unsigned int irq)
{
    (void)irq;
    timer_tick();
}

void platform_timer_init(uint32_t frequency)
{
    irq_register(PIC_IRQ_TIMER, pit_irq_handler);
    pit_set_frequency(frequency);
}
