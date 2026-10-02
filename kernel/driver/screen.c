#include <driver/screen.h>

void screen_put(struct screen *scr, uint32_t c)
{
    // Check for normal characters
    if (c >= 0x20 && c != 0x7F)
    {
        // Insert new character into the buffer
        scr->buf[scr->pos_y * SCREEN_WIDTH + scr->pos_x] = screen_glyph(c);

        // Add to the position
        scr->pos_x++;
    }
    else
    {
        // Check for special characters
        switch (c)
        {
        // Jump to a new line
        case '\n':
            scr->pos_y++;
            scr->pos_x = 0;
            break;
        // Tab character
        case '\t':
            scr->pos_x += SCREEN_TAB_SPACES;
            break;
        // Backspace character
        case '\b':
            if (scr->pos_x > 0)
                scr->pos_x--;
            else if (scr->pos_x == 0 && scr->pos_y > 0)
            {
                scr->pos_y--;
                scr->pos_x = SCREEN_WIDTH - 1;
            }
            break;
        // Carriage return character
        case '\r':
            scr->pos_x = 0;
            break;
        default:
            break;
        }
    }

    // Jump to a new line
    if (scr->pos_x >= SCREEN_WIDTH)
    {
        scr->pos_x = 0;
        scr->pos_y++;
    }

    // Scroll the screen
    if (scr->pos_y >= SCREEN_HEIGHT)
    {
        // Copy contents one live before
        memcpy(&scr->buf, &scr->buf[SCREEN_WIDTH], SCREEN_WIDTH * (SCREEN_HEIGHT - 1));

        // Clear last line
        memset(&scr->buf[SCREEN_WIDTH * (SCREEN_HEIGHT - 1)], screen_glyph(' '), SCREEN_WIDTH);

        scr->pos_y = SCREEN_HEIGHT - 1;
    }
}

void screen_init(struct screen *scr)
{
    memset(scr->buf, screen_glyph(' '), sizeof(scr->buf));
    scr->pos_x = scr->pos_y = 0;
}