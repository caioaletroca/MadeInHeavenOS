#include <driver/timer.h>
#include "io.h"

#define PIT_COMMAND 0x43
#define PIT_CHANNEL0 0x40

#define PIT_MODE0 0x30
#define PIT_MODE1 0x30
#define PIT_MODE2 0x34
#define PIT_MODE3 0x36

#define PIT_FREQUENCY 1193182

void pit_set_frequency(uint32_t frequency)
{
    uint16_t divisor = PIT_FREQUENCY / frequency;

    // Set PIT to mode 3 (square wave) with the calculated frequency
    outb(PIT_COMMAND, PIT_MODE3);

    // Send the low and high bytes of the divisor to the PIT channel 0
    outb(PIT_CHANNEL0, (uint8_t)(divisor & 0xFF));
    outb(PIT_CHANNEL0, (uint8_t)((divisor >> 8) & 0xFF));
}