#include "driver/ps2.h"

void ps2_init() {
    // TODO: Initialise USB controllers
	// TODO: Determine if the PS/2 controller exists

    // Disable devices
    ps2_disable();

    // Flush output buffer
    ps2_flush_buffer();

    // Set Configuration byte
    // Clear bits 0, 1 and 6
    // Bits 0 and 1 disable IRQs
    // Bit 6 disable translation
    uint8_t configuration_byte = ps2_unset_configuration_byte(~(uint8_t)( \
        PS2_CONFIGURATION_INTERRUPT_FIRST_PORT | \
        PS2_CONFIGURATION_INTERRUPT_SECOND_PORT | \
        PS2_CONFIGURATION_TRANSLATION \
    ));

    // Check the presence of a second PS/2 port
    if((configuration_byte & (PS2_CONFIGURATION_CLOCK_SECOND_PORT)) != 0) {
        ps2_devices[1].present = true;
    }

    // Perform controller self test
    ps2_self_test();

    // Perform device tests
    ps2_interface_test();

    ps2_keyboard_init();

    // Re-enable IRQs
    ps2_set_configuration_byte(( \
        PS2_CONFIGURATION_INTERRUPT_FIRST_PORT | \
        PS2_CONFIGURATION_INTERRUPT_SECOND_PORT
    ));

    // Enable devices
    ps2_enable();
}