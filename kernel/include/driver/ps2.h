#ifndef _PS2_H_
#define _PS2_H_

#include <stdint.h>
#include <stdbool.h>
#include <panic.h>

// PS/2 Controller I/O Ports
#define PS2_DATA        0x60
#define PS2_STATUS      0x64
#define PS2_COMMAND     0x64

// PS/2 Controller Commands
#define PS2_COMMAND_READ_BYTE_0             0x20
#define PS2_COMMAND_WRITE_BYTE_0            0x60
#define PS2_COMMAND_DISABLE_SECOND_PORT     0xA7
#define PS2_COMMAND_ENABLE_SECOND_PORT      0xA8
#define PS2_COMMAND_TEST_SECOND_PORT        0xA9
#define PS2_COMMAND_TEST                    0xAA
#define PS2_COMMAND_TEST_FIRST_PORT         0xAB
#define PS2_COMMAND_DISABLE_FIRST_PORT      0xAD
#define PS2_COMMAND_ENABLE_FIRST_PORT       0xAE

// PS/2 Controller Configuration Byte
#define PS2_CONFIGURATION_INTERRUPT_FIRST_PORT       0b00000001
#define PS2_CONFIGURATION_INTERRUPT_SECOND_PORT      0b00000010
#define PS2_CONFIGURATION_CLOCK_SECOND_PORT          0b00100000
#define PS2_CONFIGURATION_TRANSLATION                0b01000000

struct ps2_device_t {
    bool present;
    bool enabled;
};

static struct ps2_device_t ps2_devices[2];

uint8_t ps2_flush_buffer(void);
void ps2_disable(void);
void ps2_enable(void);

uint8_t ps2_set_configuration_byte(uint8_t byte);
uint8_t ps2_unset_configuration_byte(uint8_t byte);

void ps2_self_test(void);
void ps2_interface_test(void);

void ps2_init(void);

#endif