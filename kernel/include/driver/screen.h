#ifndef _SCREEN_H_
#define _SCREEN_H_

#define SCREEN_WIDTH 80
#define SCREEN_HEIGHT 25
#define SCREEN_TAB_SPACES 8

#include <stddef.h>
#include <stdint.h>
#include <string.h>

struct screen
{
    unsigned int pos_x;
    unsigned int pos_y;
    unsigned char buf[SCREEN_WIDTH * SCREEN_HEIGHT];
};

/**
 * @brief Screen initialization
 *
 * Clears the screen and set the cursor position at the start position
 *
 * @param scr
 */
void screen_init(struct screen *scr);

/**
 * @brief Puts a single character to the screen
 *
 * @param scr Screen context
 * @param c Character
 */
void screen_put(struct screen *scr, uint32_t c);

/**
 * @brief Copy the backbuffer and update the hadware cursor.
 *
 * The framebuffer has two I/O ports, one for accepting the data, and one
 * for describing the data being received. Port 0x03D4 is the port that
 * describes the data and port 0x03D5 is for the data itself.
 *
 * @param scr Screen context
 */
void screen_update(struct screen *scr);

/**
 * @brief Get the glyph corresponding to a codepoint
 *
 * @param codepoint Unicode codepoint
 * @return unsigned char Glyph
 */
unsigned char screen_glyph(uint32_t codepoint);

#endif