#include "driver/ps2.h"

void ps2_init() {
    // Initialization steps

    // Disable devices
    ps2_disable();

    // Flush output buffer
    ps2_read_data();

    // Set Configuration byte
    // We want to disable IRQs for now and disable translation
    ps2_set_configuration_byte(~( \
        PS2_CONFIGURATION_INTERRUPT_FIRST_PORT | \
        PS2_CONFIGURATION_INTERRUPT_SECOND_PORT | \
        PS2_CONFIGURATION_TRANSLATION
    ));

    // Perform controller self test
    ps2_self_test();

    ps2_keyboard_init();

    // Re-enable IRQs
    ps2_set_configuration_byte(( \
        PS2_CONFIGURATION_INTERRUPT_FIRST_PORT | \
        PS2_CONFIGURATION_INTERRUPT_SECOND_PORT
    ));

    // Enable devices
    ps2_enable();
}