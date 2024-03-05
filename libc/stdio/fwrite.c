#include "FILE.h"
#include <stdio.h>

size_t fwrite(const void *ptr, size_t size, size_t count, FILE *stream) {
    size_t n = 0, s = 0;
    const char *buf = (const char *)ptr;

    // Loop in all count of items
    while(n < count) {
        // Loop for items
        while(s < size) {
            // Write the char into stream
            fputc(buf[s], stream);
            s++;
        }
        buf += size;
        n++;
    }

    return n;
}