#include <driver/screen.h>

void screen_put(struct screen *scr, char c) {
    // Insert new character into the buffer
    scr->buf[scr->pos_y * SCREEN_WIDTH + scr->pos_x] = c;

    // Add to the position
    scr->pos_x++;

    // Jump to a new line
    if(scr->pos_x >= SCREEN_WIDTH) {
        scr->pos_x = 0;
        scr->pos_y++;
    }

    // TODO: Scroll the screen
}

void screen_write(struct screen *scr, const char *str, size_t n) {
    for(size_t i = 0; i < n; i++)
        screen_put(scr, str[i]);
}

void screen_init(struct screen *scr) {
    memset(scr->buf, ' ', sizeof(scr->buf));
    scr->pos_x = scr->pos_y = 0;
}