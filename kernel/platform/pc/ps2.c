#include <driver/ps2.h>
#include "io.h"

static inline uint8_t ps2_read_status() {
    return inb(PS2_STATUS);
}

static inline uint8_t ps2_read_data() {
    while((ps2_read_status() & 1) == 0) {}
    return inb(PS2_DATA);
}

static inline void ps2_write_data(uint8_t data) {
    while((ps2_read_status() & 2) != 0) {}
    return outb(PS2_DATA, data);
}

static inline uint8_t ps2_read_configuration_byte() {
    // Send command to read the Byte 0 on RAM
    outb(PS2_COMMAND, PS2_COMMAND_READ_BYTE_0);
    return ps2_read_data();
}

static inline void ps2_write_configuration_byte(uint8_t byte) {
    // Send command to write the Byte 0 on RAM
    outb(PS2_COMMAND, PS2_COMMAND_WRITE_BYTE_0);
    ps2_write_data(byte);
}

uint8_t ps2_flush_buffer() {
    return inb(PS2_DATA);
}

void ps2_disable() {
    outb(PS2_COMMAND, PS2_COMMAND_DISABLE_FIRST_PORT);
    outb(PS2_COMMAND, PS2_COMMAND_DISABLE_SECOND_PORT);

    ps2_devices[0].enabled = false;
    ps2_devices[1].enabled = false;
}

void ps2_enable() {
    outb(PS2_COMMAND, PS2_COMMAND_ENABLE_FIRST_PORT);
    outb(PS2_COMMAND, PS2_COMMAND_ENABLE_SECOND_PORT);

    ps2_devices[0].enabled = true;

    if(ps2_devices[1].present)
        ps2_devices[1].enabled = true;
}

uint8_t ps2_set_configuration_byte(uint8_t byte) {
    uint8_t b = ps2_read_configuration_byte() | byte;
    ps2_write_configuration_byte(b);
    return b;
}

uint8_t ps2_unset_configuration_byte(uint8_t byte) {
    uint8_t b = ps2_read_configuration_byte() & byte;
    ps2_write_configuration_byte(b);
    return b;
}

void ps2_self_test() {
    // First save the previous value of Configuration Byte
    uint8_t byte = ps2_read_configuration_byte();

    // Send the test command
    outb(PS2_COMMAND, PS2_COMMAND_TEST);

    // Wait until PS/2 Controller responds with 0x55
    if(ps2_read_data() != 0x55) {
        panic("PS/2 Controller does not respond to self test commands.\n");
    }

    // Restore old value
    ps2_write_configuration_byte(byte);
}

void ps2_interface_test() {
    outb(PS2_COMMAND, PS2_COMMAND_TEST_FIRST_PORT);
    // Wait until PS/2 Controller responds with 0x00
    if(ps2_read_data() == 0x00)
        ps2_devices[0].present = true;
            

    outb(PS2_COMMAND, PS2_COMMAND_TEST_SECOND_PORT);
    // Wait until PS/2 Controller responds with 0x00
    if(ps2_read_data() == 0x00) {
        ps2_devices[0].present = true;
    }
}