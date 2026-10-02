#include <stddef.h>
#include <stdbool.h>
#include <driver/keymap.h>

// Spacing forms of the accents, used both as dead key values and compose keys
#define ACCENT_ACUTE 0xB4     // ´
#define ACCENT_GRAVE '`'      // `
#define ACCENT_CIRCUMFLEX '^' // ^
#define ACCENT_TILDE '~'      // ~
#define ACCENT_DIAERESIS 0xA8 // ¨

/*
 * Keys typing the same character on every layout.
 *
 * Shared through macros instead of being overridden per layout: GCC warns
 * about initializing the same element twice (-Woverride-init, part of -Wextra).
 * Control keys only live in the normal level; keymap_translate falls back to
 * it when the shift or AltGr level has no entry.
 */
#define KEYMAP_COMMON_NORMAL                                                   \
    [KEY_ESCAPE] = 0x1B,                                                       \
    [KEY_BACKSPACE] = '\b',                                                    \
    [KEY_TAB] = '\t',                                                          \
    [KEY_ENTER] = '\n',                                                        \
    [KEY_SPACE] = ' ',                                                         \
                                                                               \
    [KEY_1] = '1', [KEY_2] = '2', [KEY_3] = '3', [KEY_4] = '4', [KEY_5] = '5', \
    [KEY_6] = '6', [KEY_7] = '7', [KEY_8] = '8', [KEY_9] = '9', [KEY_0] = '0', \
    [KEY_MINUS] = '-',                                                         \
    [KEY_EQUAL] = '=',                                                         \
                                                                               \
    [KEY_Q] = 'q', [KEY_W] = 'w', [KEY_E] = 'e', [KEY_R] = 'r', [KEY_T] = 't', \
    [KEY_Y] = 'y', [KEY_U] = 'u', [KEY_I] = 'i', [KEY_O] = 'o', [KEY_P] = 'p', \
    [KEY_A] = 'a', [KEY_S] = 's', [KEY_D] = 'd', [KEY_F] = 'f', [KEY_G] = 'g', \
    [KEY_H] = 'h', [KEY_J] = 'j', [KEY_K] = 'k', [KEY_L] = 'l',                \
    [KEY_Z] = 'z', [KEY_X] = 'x', [KEY_C] = 'c', [KEY_V] = 'v', [KEY_B] = 'b', \
    [KEY_N] = 'n', [KEY_M] = 'm',                                              \
    [KEY_COMMA] = ',',                                                         \
    [KEY_DOT] = '.',                                                           \
                                                                               \
    [KEY_KP_SLASH] = '/',                                                      \
    [KEY_KP_ASTERISK] = '*',                                                   \
    [KEY_KP_MINUS] = '-',                                                      \
    [KEY_KP_PLUS] = '+',                                                       \
    [KEY_KP_ENTER] = '\n',                                                     \
    [KEY_KP_0] = '0', [KEY_KP_1] = '1', [KEY_KP_2] = '2', [KEY_KP_3] = '3',    \
    [KEY_KP_4] = '4', [KEY_KP_5] = '5', [KEY_KP_6] = '6', [KEY_KP_7] = '7',    \
    [KEY_KP_8] = '8', [KEY_KP_9] = '9'

#define KEYMAP_COMMON_SHIFT                                                    \
    [KEY_1] = '!', [KEY_2] = '@', [KEY_3] = '#', [KEY_4] = '$', [KEY_5] = '%', \
    [KEY_7] = '&', [KEY_8] = '*', [KEY_9] = '(', [KEY_0] = ')',                \
    [KEY_MINUS] = '_',                                                         \
    [KEY_EQUAL] = '+',                                                         \
                                                                               \
    [KEY_Q] = 'Q', [KEY_W] = 'W', [KEY_E] = 'E', [KEY_R] = 'R', [KEY_T] = 'T', \
    [KEY_Y] = 'Y', [KEY_U] = 'U', [KEY_I] = 'I', [KEY_O] = 'O', [KEY_P] = 'P', \
    [KEY_A] = 'A', [KEY_S] = 'S', [KEY_D] = 'D', [KEY_F] = 'F', [KEY_G] = 'G', \
    [KEY_H] = 'H', [KEY_J] = 'J', [KEY_K] = 'K', [KEY_L] = 'L',                \
    [KEY_Z] = 'Z', [KEY_X] = 'X', [KEY_C] = 'C', [KEY_V] = 'V', [KEY_B] = 'B', \
    [KEY_N] = 'N', [KEY_M] = 'M',                                              \
    [KEY_COMMA] = '<',                                                         \
    [KEY_DOT] = '>'

/**
 * US (ANSI) layout. The ISO/ABNT2 extra keys get their usual US
 * fallbacks, so the layout stays usable on those keyboards.
 */
const keymap_t keymap_us = {
    .name = "us",

    .normal = {
        KEYMAP_COMMON_NORMAL,
        [KEY_GRAVE] = '`',
        [KEY_LEFT_BRACKET] = '[',
        [KEY_RIGHT_BRACKET] = ']',
        [KEY_BACKSLASH] = '\\',
        [KEY_SEMICOLON] = ';',
        [KEY_APOSTROPHE] = '\'',
        [KEY_SLASH] = '/',
        [KEY_102ND] = '\\',
        [KEY_RO] = '/',
        [KEY_KP_DOT] = '.',
        [KEY_KP_COMMA] = ',',
    },

    .shift = {
        KEYMAP_COMMON_SHIFT,
        [KEY_GRAVE] = '~',
        [KEY_6] = '^',
        [KEY_LEFT_BRACKET] = '{',
        [KEY_RIGHT_BRACKET] = '}',
        [KEY_BACKSLASH] = '|',
        [KEY_SEMICOLON] = ':',
        [KEY_APOSTROPHE] = '"',
        [KEY_SLASH] = '?',
        [KEY_102ND] = '|',
        [KEY_RO] = '?',
    },

    // No AltGr level: right Alt falls back to the normal/shift levels
    .altgr = {0},
};

/**
 * Brazilian ABNT2 layout.
 *
 * Physical positions differ from the key names: KEY_SEMICOLON is 'ç',
 * KEY_BACKSLASH is the ']' key beside Enter and KEY_SLASH is ';'.
 */
const keymap_t keymap_abnt2 = {
    .name = "abnt2",

    .normal = {
        KEYMAP_COMMON_NORMAL,
        [KEY_GRAVE] = '\'',
        [KEY_LEFT_BRACKET] = KEYMAP_DEAD_KEY(ACCENT_ACUTE),
        [KEY_RIGHT_BRACKET] = '[',
        [KEY_SEMICOLON] = 0xE7, // ç
        [KEY_APOSTROPHE] = KEYMAP_DEAD_KEY(ACCENT_TILDE),
        [KEY_BACKSLASH] = ']',
        [KEY_102ND] = '\\',
        [KEY_SLASH] = ';',
        [KEY_RO] = '/',
        [KEY_KP_DOT] = ',',
        [KEY_KP_COMMA] = '.',
    },

    .shift = {
        KEYMAP_COMMON_SHIFT,
        [KEY_GRAVE] = '"',
        [KEY_6] = KEYMAP_DEAD_KEY(ACCENT_DIAERESIS),
        [KEY_LEFT_BRACKET] = KEYMAP_DEAD_KEY(ACCENT_GRAVE),
        [KEY_RIGHT_BRACKET] = '{',
        [KEY_SEMICOLON] = 0xC7, // Ç
        [KEY_APOSTROPHE] = KEYMAP_DEAD_KEY(ACCENT_CIRCUMFLEX),
        [KEY_BACKSLASH] = '}',
        [KEY_102ND] = '|',
        [KEY_SLASH] = ':',
        [KEY_RO] = '?',
    },

    .altgr = {
        [KEY_1] = 0xB9,     // ¹
        [KEY_2] = 0xB2,     // ²
        [KEY_3] = 0xB3,     // ³
        [KEY_4] = 0xA3,     // £
        [KEY_5] = 0xA2,     // ¢
        [KEY_6] = 0xAC,     // ¬
        [KEY_EQUAL] = 0xA7, // §
        [KEY_Q] = '/',
        [KEY_W] = '?',
        [KEY_E] = 0xB0,             // °
        [KEY_RIGHT_BRACKET] = 0xAA, // ª
        [KEY_BACKSLASH] = 0xBA,     // º
        [KEY_RO] = 0xB0,            // °
    },
};

/**
 * Dead key compositions: accent followed by base gives result.
 * Any pair missing here types the accent and the base separately.
 */
static const struct
{
    uint32_t accent;
    uint32_t base;
    uint32_t result;
} keymap_compositions[] = {
    // Acute: á é í ó ú ý, plus ´c = ç as Brazilian users expect
    {ACCENT_ACUTE, 'a', 0xE1},
    {ACCENT_ACUTE, 'e', 0xE9},
    {ACCENT_ACUTE, 'i', 0xED},
    {ACCENT_ACUTE, 'o', 0xF3},
    {ACCENT_ACUTE, 'u', 0xFA},
    {ACCENT_ACUTE, 'y', 0xFD},
    {ACCENT_ACUTE, 'c', 0xE7},
    {ACCENT_ACUTE, 'A', 0xC1},
    {ACCENT_ACUTE, 'E', 0xC9},
    {ACCENT_ACUTE, 'I', 0xCD},
    {ACCENT_ACUTE, 'O', 0xD3},
    {ACCENT_ACUTE, 'U', 0xDA},
    {ACCENT_ACUTE, 'Y', 0xDD},
    {ACCENT_ACUTE, 'C', 0xC7},

    // Grave: à è ì ò ù
    {ACCENT_GRAVE, 'a', 0xE0},
    {ACCENT_GRAVE, 'e', 0xE8},
    {ACCENT_GRAVE, 'i', 0xEC},
    {ACCENT_GRAVE, 'o', 0xF2},
    {ACCENT_GRAVE, 'u', 0xF9},
    {ACCENT_GRAVE, 'A', 0xC0},
    {ACCENT_GRAVE, 'E', 0xC8},
    {ACCENT_GRAVE, 'I', 0xCC},
    {ACCENT_GRAVE, 'O', 0xD2},
    {ACCENT_GRAVE, 'U', 0xD9},

    // Circumflex: â ê î ô û
    {ACCENT_CIRCUMFLEX, 'a', 0xE2},
    {ACCENT_CIRCUMFLEX, 'e', 0xEA},
    {ACCENT_CIRCUMFLEX, 'i', 0xEE},
    {ACCENT_CIRCUMFLEX, 'o', 0xF4},
    {ACCENT_CIRCUMFLEX, 'u', 0xFB},
    {ACCENT_CIRCUMFLEX, 'A', 0xC2},
    {ACCENT_CIRCUMFLEX, 'E', 0xCA},
    {ACCENT_CIRCUMFLEX, 'I', 0xCE},
    {ACCENT_CIRCUMFLEX, 'O', 0xD4},
    {ACCENT_CIRCUMFLEX, 'U', 0xDB},

    // Tilde: ã õ ñ
    {ACCENT_TILDE, 'a', 0xE3},
    {ACCENT_TILDE, 'o', 0xF5},
    {ACCENT_TILDE, 'n', 0xF1},
    {ACCENT_TILDE, 'A', 0xC3},
    {ACCENT_TILDE, 'O', 0xD5},
    {ACCENT_TILDE, 'N', 0xD1},

    // Diaeresis: ä ë ï ö ü ÿ
    {ACCENT_DIAERESIS, 'a', 0xE4},
    {ACCENT_DIAERESIS, 'e', 0xEB},
    {ACCENT_DIAERESIS, 'i', 0xEF},
    {ACCENT_DIAERESIS, 'o', 0xF6},
    {ACCENT_DIAERESIS, 'u', 0xFC},
    {ACCENT_DIAERESIS, 'y', 0xFF},
    {ACCENT_DIAERESIS, 'A', 0xC4},
    {ACCENT_DIAERESIS, 'E', 0xCB},
    {ACCENT_DIAERESIS, 'I', 0xCF},
    {ACCENT_DIAERESIS, 'O', 0xD6},
    {ACCENT_DIAERESIS, 'U', 0xDC},
};

/**
 * Keypad keys that type characters only while Num Lock is on; otherwise
 * they act as navigation keys (Home, Up, PgUp, ...) and type nothing.
 */
static bool keymap_is_numlock_key(keycode_t key)
{
    return (key >= KEY_KP_0 && key <= KEY_KP_9) || key == KEY_KP_DOT;
}

/**
 * Lowercase letters Caps Lock applies to: a-z and Latin-1 à..þ except ÷.
 */
static bool keymap_is_lowercase(uint32_t c)
{
    return (c >= 'a' && c <= 'z') || (c >= 0xE0 && c <= 0xFE && c != 0xF7);
}

codepoint_t keymap_translate(const keymap_t *map, keycode_t key, unsigned int modifiers)
{
    if (key == KEY_NONE || key >= KEY_COUNT)
        return 0;

    if (!(modifiers & KEYMAP_NUM_LOCK) && keymap_is_numlock_key(key))
        return 0;

    bool shift = modifiers & KEYMAP_SHIFT;

    // Caps Lock acts as Shift on letters only, and Shift undoes it (Caps + Shift + a = a)
    if ((modifiers & KEYMAP_CAPS_LOCK) && keymap_is_lowercase(map->normal[key]))
        shift = !shift;

    codepoint_t c = 0;

    // Most specific level first; an empty entry falls back to the next one
    if (modifiers & KEYMAP_ALTGR)
        c = map->altgr[key];
    if (c == 0 && shift)
        c = map->shift[key];
    if (c == 0)
        c = map->normal[key];

    return c;
}

codepoint_t keymap_compose(codepoint_t accent, codepoint_t base)
{
    for (size_t i = 0; i < sizeof(keymap_compositions) / sizeof(keymap_compositions[0]); i++)
    {
        if (keymap_compositions[i].accent == accent && keymap_compositions[i].base == base)
            return keymap_compositions[i].result;
    }

    return 0;
}