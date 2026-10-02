#ifndef _DRIVER_KEYMAP_H
#define _DRIVER_KEYMAP_H

#include <stdint.h>
#include <unicode.h>
#include <driver/input.h>

#define KEYMAP_DEAD 0x80000000u // bit 31: dead key, low bits = accent
#define KEYMAP_DEAD_KEY(accent) (KEYMAP_DEAD | (accent))

#define KEYMAP_SHIFT (1u << 0)
#define KEYMAP_ALTGR (1u << 1)
#define KEYMAP_CAPS_LOCK (1u << 2)
#define KEYMAP_NUM_LOCK (1u << 3)

typedef struct keymap
{
    const char *name;
    uint32_t normal[KEY_COUNT];
    uint32_t shift[KEY_COUNT];
    uint32_t altgr[KEY_COUNT];
} keymap_t;

extern const keymap_t keymap_us;
extern const keymap_t keymap_abnt2;

/**
 * Translate a keycode and modifiers into a character using the specified keymap.
 *
 * @param map The keymap to use for translation.
 * @param key The keycode to translate.
 * @param modifiers The active modifier keys (e.g., shift, altgr).
 * @return The Unicode code point the key types, KEYMAP_DEAD | accent for a
 *         dead key, or 0 if the key types nothing (modifiers, F-keys, arrows,
 *         keypad digits with Num Lock off).
 */
codepoint_t keymap_translate(const keymap_t *map, keycode_t key, unsigned int modifiers);

/**
 * Compose a character from an accent and a base character.
 *
 * @param accent The accent character.
 * @param base The base character.
 * @return The composed character.
 */
codepoint_t keymap_compose(codepoint_t accent, codepoint_t base);

#endif // _DRIVER_KEYMAP_H