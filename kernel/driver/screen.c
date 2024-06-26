#include <driver/screen.h>

void screen_put(struct screen *scr, char c) {
    // Check for normal characters
    if(' ' <= c && c <= '~') {
        // Insert new character into the buffer
        scr->buf[scr->pos_y * SCREEN_WIDTH + scr->pos_x] = c;

        // Add to the position
        scr->pos_x++;
    }
    else {
        // Check for special characters
        switch (c)
        {
            // Jump to a new line
            case '\n':
                scr->pos_y++;
                scr->pos_x = 0;
                break;
            case '\t':
                scr->pos_x += SCREEN_TAB_SPACES;
                break;
            default:
                break;
        }
    }


    // Jump to a new line
    if(scr->pos_x >= SCREEN_WIDTH) {
        scr->pos_x = 0;
        scr->pos_y++;
    }

    // Scroll the screen
    if(scr->pos_y >= SCREEN_HEIGHT) {
        // Copy contents one live before
        memcpy(&scr->buf, &scr->buf[SCREEN_WIDTH], SCREEN_WIDTH * (SCREEN_HEIGHT - 1));
        
        // Clear last line
        memset(&scr->buf[SCREEN_WIDTH * (SCREEN_HEIGHT - 1)], ' ', SCREEN_WIDTH);

        scr->pos_y = SCREEN_HEIGHT - 1;
    }
}

void screen_write(struct screen *scr, const char *str, size_t n) {
    for(size_t i = 0; i < n; i++)
        screen_put(scr, str[i]);
}

void screen_init(struct screen *scr) {
    memset(scr->buf, ' ', sizeof(scr->buf));
    scr->pos_x = scr->pos_y = 0;
}