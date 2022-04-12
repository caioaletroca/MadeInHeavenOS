#include <kernel/tty.h>
#include <stdint.h>
#include <string.h>

#define VIDEO_BUF   ((uint16_t *) (0xB8000 + KVBASE))

#include "vga.h"
#include "screen.h"

static struct screen *scr;

static int tty_putchar(char c) {
    tty_init();
    screen_put(&scr, c);
    uint16_t *buf = (uint16_t *)VIDEO_BUF;

    for (i = 0; i < sizeof(scr->buf); i++)
        buf[i] = vga_entry(c, vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
}

static tty_init() {
    screen_init(&scr);
}

// static size_t terminal_row;
// static size_t terminal_column;
// static uint8_t terminal_color;

// void terminal_writestring(const char *data) {
//     terminal_write(data, strlen(data));
// }

// void terminal_write(const char *data, size_t size) {
//     outb(0x03D4, 65);
//     uint8_t test = inb(0x03D4);
//     terminal_put(test);
//     for(size_t i = 0; i < size; i++)
//         terminal_put(data[i]);
// }

// void terminal_put(char c) {
//     unsigned char uc = c;
//     terminal_putat(uc, terminal_color, terminal_column, terminal_row);
//     if(++terminal_column == VGA_WIDTH) {
//         terminal_column = 0;
//         if(++terminal_row == VGA_HEIGHT)
//             terminal_row = 0;
//     }
// }

// void terminal_putat(unsigned char c, uint8_t color, size_t x, size_t y) {
//     const size_t index = y * VGA_WIDTH + x;
//     terminal_buffer[index] = vga_entry(c, color);
// }


// void terminal_clear(void) {
//     terminal_row = 0;
//     terminal_column = 0;
//     size_t screen_size = VGA_WIDTH * VGA_HEIGHT;
//     terminal_color = vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
//     for(size_t i = 0; i < screen_size; i++) {
//         terminal_buffer[i] = vga_entry(' ', terminal_color);
//     }
// }