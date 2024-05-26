#include "driver/keyboard.h"
#include "isr.h"
// #include "io.h"

#define out_asm(port, value) __asm__ __volatile__ ("out %b0, %w1" : : "a" (value), "Nd" (port))

#define in_asm(port, value) __asm__ __volatile__ ("in %w1, %b0" : "=a" (value) : "Nd" (port))

/**
 * @brief Write on byte to an output port
 * 
 * @param port Port address
 * @param value Value
 */
static inline void outb(uint16_t port, uint8_t value) {
    out_asm(port, value);
}

/**
 * @brief Read one byte from an input port
 * 
 * @param port Port address
 * @return uint8_t
 */
static inline uint8_t inb(uint16_t port) {
    uint8_t value;
    in_asm(port, value);
    return value;
}

#define PS2_STATUS 0x64
#define PS2_COMMAND 0x64
#define PS2_DATA 0x60

void ps2_keyboard_irq_handler(isr_context *regs) {
    while ((inb(PS2_STATUS) & 1) == 0);
	uint8_t casa = inb(PS2_DATA);

    kprintf("SCANCODE SET 2: 0x%X\n", casa);

    outb(0x20, 0x20);
}

void ps2_keyboard_init() {


    isr_info_t ps2_keyboard_info = {
        .type = ISR_IRQ,
        .handler = ps2_keyboard_irq_handler,
    };

    isr_set_info(33, &ps2_keyboard_info);
}