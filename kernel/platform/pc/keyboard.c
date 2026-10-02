#include "keyboard.h"
#include <arch/irq.h>
#include "ps2.h"
#include <driver/input.h>
#include <x86/io.h>
#include "pic.h"

// Scancode set 2 prefixes
#define SC2_EXTENDED 0xE0
#define SC2_RELEASE 0xF0
#define SC2_PAUSE 0xE1

// Pause sends E1 14 77 E1 F0 14 F0 77 and never a release code
#define SC2_PAUSE_SEQUENCE_LENGTH 8

#define ARRAY_SIZE(array) (sizeof(array) / sizeof((array)[0]))

/**
 * Scancode set 2, single-byte make codes.
 */
static const keycode_t set2_keymap[0x84] = {
    // Function row
    [0x76] = KEY_ESCAPE,
    [0x05] = KEY_F1,
    [0x06] = KEY_F2,
    [0x04] = KEY_F3,
    [0x0C] = KEY_F4,
    [0x03] = KEY_F5,
    [0x0B] = KEY_F6,
    [0x83] = KEY_F7,
    [0x0A] = KEY_F8,
    [0x01] = KEY_F9,
    [0x09] = KEY_F10,
    [0x78] = KEY_F11,
    [0x07] = KEY_F12,
    [0x7E] = KEY_SCROLL_LOCK,

    // Number row
    [0x0E] = KEY_GRAVE,
    [0x16] = KEY_1,
    [0x1E] = KEY_2,
    [0x26] = KEY_3,
    [0x25] = KEY_4,
    [0x2E] = KEY_5,
    [0x36] = KEY_6,
    [0x3D] = KEY_7,
    [0x3E] = KEY_8,
    [0x46] = KEY_9,
    [0x45] = KEY_0,
    [0x4E] = KEY_MINUS,
    [0x55] = KEY_EQUAL,
    [0x66] = KEY_BACKSPACE,

    // Top letter row
    [0x0D] = KEY_TAB,
    [0x15] = KEY_Q,
    [0x1D] = KEY_W,
    [0x24] = KEY_E,
    [0x2D] = KEY_R,
    [0x2C] = KEY_T,
    [0x35] = KEY_Y,
    [0x3C] = KEY_U,
    [0x43] = KEY_I,
    [0x44] = KEY_O,
    [0x4D] = KEY_P,
    [0x54] = KEY_LEFT_BRACKET,
    [0x5B] = KEY_RIGHT_BRACKET,
    [0x5D] = KEY_BACKSLASH,

    // Home row
    [0x58] = KEY_CAPS_LOCK,
    [0x1C] = KEY_A,
    [0x1B] = KEY_S,
    [0x23] = KEY_D,
    [0x2B] = KEY_F,
    [0x34] = KEY_G,
    [0x33] = KEY_H,
    [0x3B] = KEY_J,
    [0x42] = KEY_K,
    [0x4B] = KEY_L,
    [0x4C] = KEY_SEMICOLON,
    [0x52] = KEY_APOSTROPHE,
    [0x5A] = KEY_ENTER,

    // Bottom row
    [0x12] = KEY_LEFT_SHIFT,
    [0x61] = KEY_102ND,
    [0x1A] = KEY_Z,
    [0x22] = KEY_X,
    [0x21] = KEY_C,
    [0x2A] = KEY_V,
    [0x32] = KEY_B,
    [0x31] = KEY_N,
    [0x3A] = KEY_M,
    [0x41] = KEY_COMMA,
    [0x49] = KEY_DOT,
    [0x4A] = KEY_SLASH,
    [0x51] = KEY_RO,
    [0x59] = KEY_RIGHT_SHIFT,

    // Modifiers and space
    [0x14] = KEY_LEFT_CTRL,
    [0x11] = KEY_LEFT_ALT,
    [0x29] = KEY_SPACE,

    // Keypad
    [0x77] = KEY_NUM_LOCK,
    [0x7C] = KEY_KP_ASTERISK,
    [0x7B] = KEY_KP_MINUS,
    [0x79] = KEY_KP_PLUS,
    [0x71] = KEY_KP_DOT,
    [0x6D] = KEY_KP_COMMA,
    [0x70] = KEY_KP_0,
    [0x69] = KEY_KP_1,
    [0x72] = KEY_KP_2,
    [0x7A] = KEY_KP_3,
    [0x6B] = KEY_KP_4,
    [0x73] = KEY_KP_5,
    [0x74] = KEY_KP_6,
    [0x6C] = KEY_KP_7,
    [0x75] = KEY_KP_8,
    [0x7D] = KEY_KP_9,
};

/**
 * Scancode set 2, make codes following the E0 prefix.
 *
 * E0 12 and E0 59 are "fake shifts" the keyboard wraps around Print Screen
 * and the navigation keys; they are left unmapped so they are ignored.
 */
static const keycode_t set2_extended_keymap[0x80] = {
    // Modifiers
    [0x14] = KEY_RIGHT_CTRL,
    [0x11] = KEY_RIGHT_ALT,
    [0x1F] = KEY_LEFT_META,
    [0x27] = KEY_RIGHT_META,
    [0x2F] = KEY_MENU,

    // Print Screen (make: E0 12 E0 7C, break: E0 F0 7C E0 F0 12)
    [0x7C] = KEY_PRINT_SCREEN,

    // Navigation block
    [0x70] = KEY_INSERT,
    [0x71] = KEY_DELETE,
    [0x6C] = KEY_HOME,
    [0x69] = KEY_END,
    [0x7D] = KEY_PAGE_UP,
    [0x7A] = KEY_PAGE_DOWN,
    [0x75] = KEY_UP,
    [0x72] = KEY_DOWN,
    [0x6B] = KEY_LEFT,
    [0x74] = KEY_RIGHT,

    // Keypad
    [0x4A] = KEY_KP_SLASH,
    [0x5A] = KEY_KP_ENTER,

    // Media
    [0x23] = KEY_MUTE,
    [0x21] = KEY_VOLUME_DOWN,
    [0x32] = KEY_VOLUME_UP,
    [0x34] = KEY_PLAY_PAUSE,
    [0x3B] = KEY_MEDIA_STOP,
    [0x15] = KEY_PREVIOUS_TRACK,
    [0x4D] = KEY_NEXT_TRACK,

    // ACPI
    [0x37] = KEY_POWER,
    [0x3F] = KEY_SLEEP,
    [0x5E] = KEY_WAKE,
};

static void ps2_keyboard_irq_handler(unsigned int irq)
{
    // Decoder state persists across IRQs: a key can span several bytes
    static bool extended = false;
    static bool release = false;
    static unsigned int pause_remaining = 0;

    (void)irq;
    uint8_t byte = inb(PS2_DATA);

    // Swallow the rest of the Pause sequence
    if (pause_remaining > 0)
    {
        pause_remaining--;
        return;
    }

    if (byte == SC2_PAUSE)
    {
        // Pause has no break code: report a full press/release right away
        pause_remaining = SC2_PAUSE_SEQUENCE_LENGTH - 1;
        input_report_key(KEY_PAUSE, true);
        input_report_key(KEY_PAUSE, false);
        return;
    }

    if (byte == SC2_EXTENDED)
    {
        extended = true;
        return;
    }

    if (byte == SC2_RELEASE)
    {
        release = true;
        return;
    }

    // Anything else (including controller replies like 0xFA ACK) is out of
    // range of both tables and maps to KEY_NONE
    keycode_t key = KEY_NONE;

    if (extended && byte < ARRAY_SIZE(set2_extended_keymap))
        key = set2_extended_keymap[byte];
    else if (!extended && byte < ARRAY_SIZE(set2_keymap))
        key = set2_keymap[byte];

    if (key != KEY_NONE)
        input_report_key(key, !release);

    extended = false;
    release = false;
}

void ps2_keyboard_init(void)
{
    irq_register(PIC_IRQ_KEYBOARD, ps2_keyboard_irq_handler);
}
