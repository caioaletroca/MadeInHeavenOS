#include <driver/ps2.h>
#include <kprintf.h>
#include "io.h"

uint8_t ps2_read_data() {
    while(inb(PS2_STATUS) & 1 != 0);
    return inb(PS2_DATA);
}

void ps2_disable() {
    outb(PS2_COMMAND, PS2_COMMAND_DISABLE_FIRST_PORT);
    outb(PS2_COMMAND, PS2_COMMAND_DISABLE_SECOND_PORT);
}

void ps2_enable() {
    outb(PS2_COMMAND, PS2_COMMAND_ENABLE_FIRST_PORT);
    outb(PS2_COMMAND, PS2_COMMAND_ENABLE_SECOND_PORT);
}

void ps2_set_configuration_byte(uint8_t byte) {
    // Send command to read the Byte 0 on RAM
    outb(PS2_COMMAND, PS2_COMMAND_READ_BYTE_0);
    uint8_t configuration_byte = ps2_read_data();

    // Clear bits 0, 1 and 6
    // Bits 0 and 1 disable IRQs
    // Bit 6 disable translation
    configuration_byte &= byte;

    // Write the new configuration byte back
    outb(PS2_COMMAND, PS2_COMMAND_WRITE_BYTE_0);
}

void ps2_self_test() {
    // First save the previous value on PS2 Command register
    uint8_t value = inb(PS2_COMMAND);

    // Send the test command
    outb(PS2_COMMAND, PS2_COMMAND_TEST);

    // Wait until PS/2 Controller responds with 0x55
    int attempts = 0;
    while(inb(PS2_DATA) != 0x55 || attempts < 100) {
        attempts++;
    }

    // PS/2 controller fails
    if(attempts == 100) {
        panic("PS/2 Controller does not respond to self test commands.");
    }

    // Restore old value
    outb(PS2_COMMAND, value);
}