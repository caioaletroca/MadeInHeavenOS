#include <x86/io.h>
#include "vga.h"
#include "driver/screen.h"
#include <addresses.h>

void screen_update(struct screen *scr)
{
    uint16_t pos = scr->pos_y * SCREEN_WIDTH + scr->pos_x;
    uint16_t *buf = phys_to_kern(VGA_ADDRESS);
    uint8_t color = vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);

    for (size_t i = 0; i < sizeof(scr->buf); i++)
    {
        buf[i] = vga_entry(scr->buf[i], color);
    }

    vga_set_cursor(pos);
}

// Drawn for code points the VGA font (code page 437) cannot show
#define SCREEN_GLYPH_UNKNOWN '?'

/**
 * Latin-1 Supplement (U+00A0..U+00FF) to CP437 glyphs.
 *
 * Characters CP437 lacks fall back to the closest ASCII character, usually
 * the base letter without its accent (ã -> a, Ó -> O).
 */
static const unsigned char latin1_glyphs[0x60] = {
    0xFF,
    0xAD,
    0x9B,
    0x9C,
    '?',
    0x9D,
    '|',
    0x15, // U+00A0  nbsp ¡ ¢ £ ¤ ¥ ¦ §
    '"',
    'c',
    0xA6,
    0xAE,
    0xAA,
    '-',
    'r',
    '-', // U+00A8  ¨ © ª « ¬ shy ® ¯
    0xF8,
    0xF1,
    0xFD,
    '3',
    '\'',
    0xE6,
    0x14,
    0xFA, // U+00B0  ° ± ² ³ ´ µ ¶ ·
    ',',
    '1',
    0xA7,
    0xAF,
    0xAC,
    0xAB,
    '?',
    0xA8, // U+00B8  ¸ ¹ º » ¼ ½ ¾ ¿
    'A',
    'A',
    'A',
    'A',
    0x8E,
    0x8F,
    0x92,
    0x80, // U+00C0  À Á Â Ã Ä Å Æ Ç
    'E',
    0x90,
    'E',
    'E',
    'I',
    'I',
    'I',
    'I', // U+00C8  È É Ê Ë Ì Í Î Ï
    'D',
    0xA5,
    'O',
    'O',
    'O',
    'O',
    0x99,
    'x', // U+00D0  Ð Ñ Ò Ó Ô Õ Ö ×
    'O',
    'U',
    'U',
    'U',
    0x9A,
    'Y',
    '?',
    0xE1, // U+00D8  Ø Ù Ú Û Ü Ý Þ ß
    0x85,
    0xA0,
    0x83,
    'a',
    0x84,
    0x86,
    0x91,
    0x87, // U+00E0  à á â ã ä å æ ç
    0x8A,
    0x82,
    0x88,
    0x89,
    0x8D,
    0xA1,
    0x8C,
    0x8B, // U+00E8  è é ê ë ì í î ï
    'd',
    0xA4,
    0x95,
    0xA2,
    0x93,
    'o',
    0x94,
    0xF6, // U+00F0  ð ñ ò ó ô õ ö ÷
    'o',
    0x97,
    0xA3,
    0x96,
    0x81,
    'y',
    '?',
    0x98, // U+00F8  ø ù ú û ü ý þ ÿ
};

/**
 * Remaining CP437 glyphs outside ASCII and Latin-1: symbols drawn by the
 * control range (0x01..0x1F, 0x7F), Greek, math and box drawing.
 *
 * Sorted by code point for binary search.
 */
static const struct
{
    uint16_t codepoint;
    uint8_t glyph;
} cp437_glyphs[] = {
    {0x0192, 0x9F}, // ƒ
    {0x0393, 0xE2}, // Γ
    {0x0398, 0xE9}, // Θ
    {0x03A3, 0xE4}, // Σ
    {0x03A6, 0xE8}, // Φ
    {0x03A9, 0xEA}, // Ω
    {0x03B1, 0xE0}, // α
    {0x03B4, 0xEB}, // δ
    {0x03B5, 0xEE}, // ε
    {0x03C0, 0xE3}, // π
    {0x03C3, 0xE5}, // σ
    {0x03C4, 0xE7}, // τ
    {0x03C6, 0xED}, // φ
    {0x2022, 0x07}, // •
    {0x203C, 0x13}, // ‼
    {0x207F, 0xFC}, // ⁿ
    {0x20A7, 0x9E}, // ₧
    {0x2190, 0x1B}, // ←
    {0x2191, 0x18}, // ↑
    {0x2192, 0x1A}, // →
    {0x2193, 0x19}, // ↓
    {0x2194, 0x1D}, // ↔
    {0x2195, 0x12}, // ↕
    {0x21A8, 0x17}, // ↨
    {0x2219, 0xF9}, // ∙
    {0x221A, 0xFB}, // √
    {0x221E, 0xEC}, // ∞
    {0x221F, 0x1C}, // ∟
    {0x2229, 0xEF}, // ∩
    {0x2248, 0xF7}, // ≈
    {0x2261, 0xF0}, // ≡
    {0x2264, 0xF3}, // ≤
    {0x2265, 0xF2}, // ≥
    {0x2302, 0x7F}, // ⌂
    {0x2310, 0xA9}, // ⌐
    {0x2320, 0xF4}, // ⌠
    {0x2321, 0xF5}, // ⌡
    {0x2500, 0xC4}, // ─
    {0x2502, 0xB3}, // │
    {0x250C, 0xDA}, // ┌
    {0x2510, 0xBF}, // ┐
    {0x2514, 0xC0}, // └
    {0x2518, 0xD9}, // ┘
    {0x251C, 0xC3}, // ├
    {0x2524, 0xB4}, // ┤
    {0x252C, 0xC2}, // ┬
    {0x2534, 0xC1}, // ┴
    {0x253C, 0xC5}, // ┼
    {0x2550, 0xCD}, // ═
    {0x2551, 0xBA}, // ║
    {0x2552, 0xD5}, // ╒
    {0x2553, 0xD6}, // ╓
    {0x2554, 0xC9}, // ╔
    {0x2555, 0xB8}, // ╕
    {0x2556, 0xB7}, // ╖
    {0x2557, 0xBB}, // ╗
    {0x2558, 0xD4}, // ╘
    {0x2559, 0xD3}, // ╙
    {0x255A, 0xC8}, // ╚
    {0x255B, 0xBE}, // ╛
    {0x255C, 0xBD}, // ╜
    {0x255D, 0xBC}, // ╝
    {0x255E, 0xC6}, // ╞
    {0x255F, 0xC7}, // ╟
    {0x2560, 0xCC}, // ╠
    {0x2561, 0xB5}, // ╡
    {0x2562, 0xB6}, // ╢
    {0x2563, 0xB9}, // ╣
    {0x2564, 0xD1}, // ╤
    {0x2565, 0xD2}, // ╥
    {0x2566, 0xCB}, // ╦
    {0x2567, 0xCF}, // ╧
    {0x2568, 0xD0}, // ╨
    {0x2569, 0xCA}, // ╩
    {0x256A, 0xD8}, // ╪
    {0x256B, 0xD7}, // ╫
    {0x256C, 0xCE}, // ╬
    {0x2580, 0xDF}, // ▀
    {0x2584, 0xDC}, // ▄
    {0x2588, 0xDB}, // █
    {0x258C, 0xDD}, // ▌
    {0x2590, 0xDE}, // ▐
    {0x2591, 0xB0}, // ░
    {0x2592, 0xB1}, // ▒
    {0x2593, 0xB2}, // ▓
    {0x25A0, 0xFE}, // ■
    {0x25AC, 0x16}, // ▬
    {0x25B2, 0x1E}, // ▲
    {0x25BA, 0x10}, // ►
    {0x25BC, 0x1F}, // ▼
    {0x25C4, 0x11}, // ◄
    {0x25CB, 0x09}, // ○
    {0x25D8, 0x08}, // ◘
    {0x25D9, 0x0A}, // ◙
    {0x263A, 0x01}, // ☺
    {0x263B, 0x02}, // ☻
    {0x263C, 0x0F}, // ☼
    {0x2640, 0x0C}, // ♀
    {0x2642, 0x0B}, // ♂
    {0x2660, 0x06}, // ♠
    {0x2663, 0x05}, // ♣
    {0x2665, 0x03}, // ♥
    {0x2666, 0x04}, // ♦
    {0x266A, 0x0D}, // ♪
    {0x266B, 0x0E}, // ♫
};

unsigned char screen_glyph(uint32_t codepoint)
{
    // ASCII glyphs share their code
    if (codepoint >= 0x20 && codepoint < 0x7F)
        return (unsigned char)codepoint;

    if (codepoint >= 0xA0 && codepoint <= 0xFF)
        return latin1_glyphs[codepoint - 0xA0];

    size_t low = 0;
    size_t high = sizeof(cp437_glyphs) / sizeof(cp437_glyphs[0]);

    while (low < high)
    {
        size_t middle = low + (high - low) / 2;

        if (cp437_glyphs[middle].codepoint == codepoint)
            return cp437_glyphs[middle].glyph;

        if (cp437_glyphs[middle].codepoint < codepoint)
            low = middle + 1;
        else
            high = middle;
    }

    return SCREEN_GLYPH_UNKNOWN;
}