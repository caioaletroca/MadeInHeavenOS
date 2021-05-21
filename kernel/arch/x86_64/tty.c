#include <kernel/tty.h>
#include <stdint.h>

#include "vga.h"

static uint8_t terminal_color;
static uint16_t *terminal_buffer = (uint16_t *) VGA_ADDRESS;

void terminal_clear(void) {
    size_t screen_size = VGA_WIDTH * VGA_HEIGHT;
    terminal_color = vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    for(size_t i = 0; i < screen_size; i++) {
        terminal_buffer[i] = vga_entry(' ', terminal_color);
    }
}