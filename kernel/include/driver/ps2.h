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

/**
 * Flushs the buffer by reading data register without checking status
*/
uint8_t ps2_flush_buffer(void);

/**
 * Disable both ports using commands and disabling device struct
*/
void ps2_disable(void);

/**
 * Enable both ports using command and enabling device struct
*/
void ps2_enable(void);

/**
 * Sets the current configuration byte using a OR bit mask
*/
uint8_t ps2_set_configuration_byte(uint8_t byte);

/**
 * Unsets the current configuration byte using a AND bit mask
*/
uint8_t ps2_unset_configuration_byte(uint8_t byte);

/**
 * Performs a PS/2 Controller self test
*/
void ps2_self_test(void);

/**
 * Performs a PS/2 interface test on both ports
*/
void ps2_interface_test(void);

/**
 * Initialize PS/2 system
*/
void ps2_init(void);

#endif