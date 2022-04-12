#ifndef _SCREEN_H_
#define _SCREEN_H_

#define SCREEN_WIDTH    80
#define SCREEN_HEIGHT   25

#include <stddef.h>
#include <string.h>

struct screen {
    unsigned int pos_x;
    unsigned int pos_y;
    char buf[SCREEN_WIDTH * SCREEN_HEIGHT];
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
void screen_put(struct screen *scr, char c);

/**
 * @brief Write a character string to the screen
 * 
 * @param scr Screen context
 * @param str Characters string
 * @param n String length
 */
void screen_write(struct screen *scr, const char *str, size_t n);

#endif