#include <driver/tty.h>

static struct screen scr;

static int tty_putchar(char c) {
    screen_put(&scr, c);
    return 0;
}

size_t tty_write(const void *str, size_t n) {
    size_t i;
    for(i = 0; i < n; i++)
        if(tty_putchar(((const unsigned char *)str)[i]) < 0)
            break;
            
    // TODO: Adjust timers for screen update
    screen_update(&scr);
    return i;
}

void tty_init() {
    screen_init(&scr);
}