#include "io.h"
#include "vga.h"
#include "driver/screen.h"

void screen_update(struct screen *scr) {
    uint16_t pos = scr->pos_y * SCREEN_WIDTH + scr->pos_x;
    uint16_t *buf = (uint16_t *)VGA_ADDRESS;
    uint8_t color = vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);

    for(size_t i = 0; i < sizeof(scr->buf); i++) {
        buf[i] = vga_entry(scr->buf[i], color);
    }
}