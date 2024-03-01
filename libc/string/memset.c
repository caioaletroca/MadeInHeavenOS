#include <string.h>

void *memset(void *str, int c, size_t n) {
    char* buf = (char*) str;

    for(size_t i = 0; i < n; i++) {
        buf[i] = c;

        // TODO: DIRT HACK ALERT!!!
        // Find a way to fix GCC infinity loop without this
        __asm__ __volatile__ ("nop");
    }

    return str;
}