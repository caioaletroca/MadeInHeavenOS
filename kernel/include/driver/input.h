#ifndef _INPUT_H_
#define _INPUT_H_

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Hardware-independent key codes.
 *
 * They name physical key positions on a US-style layout, not characters:
 * KEY_SEMICOLON is the key right of L, which types 'ç' on ABNT2. Turning
 * keys into characters is the job of the keymap in the console layer.
 */
typedef enum keycode
{
    KEY_NONE = 0,

    // Main block, row by row
    KEY_ESCAPE,
    KEY_GRAVE,
    KEY_1,
    KEY_2,
    KEY_3,
    KEY_4,
    KEY_5,
    KEY_6,
    KEY_7,
    KEY_8,
    KEY_9,
    KEY_0,
    KEY_MINUS,
    KEY_EQUAL,
    KEY_BACKSPACE,

    KEY_TAB,
    KEY_Q,
    KEY_W,
    KEY_E,
    KEY_R,
    KEY_T,
    KEY_Y,
    KEY_U,
    KEY_I,
    KEY_O,
    KEY_P,
    KEY_LEFT_BRACKET,
    KEY_RIGHT_BRACKET,
    KEY_BACKSLASH,

    KEY_CAPS_LOCK,
    KEY_A,
    KEY_S,
    KEY_D,
    KEY_F,
    KEY_G,
    KEY_H,
    KEY_J,
    KEY_K,
    KEY_L,
    KEY_SEMICOLON,
    KEY_APOSTROPHE,
    KEY_ENTER,

    KEY_LEFT_SHIFT,
    KEY_102ND, // ISO/ABNT2 extra key left of Z ('\' / '|' on ABNT2)
    KEY_Z,
    KEY_X,
    KEY_C,
    KEY_V,
    KEY_B,
    KEY_N,
    KEY_M,
    KEY_COMMA,
    KEY_DOT,
    KEY_SLASH,
    KEY_RO, // ABNT2 extra key left of right Shift ('/' / '?')
    KEY_RIGHT_SHIFT,

    KEY_LEFT_CTRL,
    KEY_LEFT_META,
    KEY_LEFT_ALT,
    KEY_SPACE,
    KEY_RIGHT_ALT, // AltGr on international layouts
    KEY_RIGHT_META,
    KEY_MENU,
    KEY_RIGHT_CTRL,

    // Function row
    KEY_F1,
    KEY_F2,
    KEY_F3,
    KEY_F4,
    KEY_F5,
    KEY_F6,
    KEY_F7,
    KEY_F8,
    KEY_F9,
    KEY_F10,
    KEY_F11,
    KEY_F12,
    KEY_PRINT_SCREEN,
    KEY_SCROLL_LOCK,
    KEY_PAUSE,

    // Navigation block
    KEY_INSERT,
    KEY_DELETE,
    KEY_HOME,
    KEY_END,
    KEY_PAGE_UP,
    KEY_PAGE_DOWN,
    KEY_UP,
    KEY_DOWN,
    KEY_LEFT,
    KEY_RIGHT,

    // Keypad
    KEY_NUM_LOCK,
    KEY_KP_SLASH,
    KEY_KP_ASTERISK,
    KEY_KP_MINUS,
    KEY_KP_PLUS,
    KEY_KP_ENTER,
    KEY_KP_DOT,
    KEY_KP_COMMA, // ABNT2 extra keypad key ('.')
    KEY_KP_0,
    KEY_KP_1,
    KEY_KP_2,
    KEY_KP_3,
    KEY_KP_4,
    KEY_KP_5,
    KEY_KP_6,
    KEY_KP_7,
    KEY_KP_8,
    KEY_KP_9,

    // Media and ACPI
    KEY_MUTE,
    KEY_VOLUME_DOWN,
    KEY_VOLUME_UP,
    KEY_PLAY_PAUSE,
    KEY_MEDIA_STOP,
    KEY_PREVIOUS_TRACK,
    KEY_NEXT_TRACK,
    KEY_POWER,
    KEY_SLEEP,
    KEY_WAKE,

    KEY_COUNT
} keycode_t;

/**
 * @brief Structure representing an input event.
 */
typedef struct input_event
{
    keycode_t key;
    bool pressed;
} input_event_t;

/**
 * @brief Initialize the input subsystem.
 *
 * This function should be called during system initialization to set up the input subsystem.
 */
void input_init(void);

/**
 * @brief Report a key event to the input subsystem.
 *
 * @param key The keycode of the key event.
 * @param pressed True if the key is pressed, false if released.
 */
void input_report_key(keycode_t key, bool pressed);

/**
 * @brief Retrieve the next input event from the input subsystem.
 *
 * @return The next input event.
 */
input_event_t input_get_event(void);

#endif // _INPUT_H_