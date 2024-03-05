#ifndef _PS2_H_
#define _PS2_H_

#include <stdint.h>
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
#define PS2_COMMAND_TEST                    0xAA
#define PS2_COMMAND_DISABLE_FIRST_PORT      0xAD
#define PS2_COMMAND_ENABLE_FIRST_PORT       0xAE

// PS/2 Controller Configuration Byte
#define PS2_CONFIGURATION_INTERRUPT_FIRST_PORT       0x00000001
#define PS2_CONFIGURATION_INTERRUPT_SECOND_PORT      0x00000010
#define PS2_CONFIGURATION_TRANSLATION                0x00100000

uint8_t ps2_read_data(void);

void ps2_disable(void);

void ps2_enable(void);

void ps2_set_configuration_byte(uint8_t byte);

void ps2_self_test(void);

void ps2_init(void);

#endif