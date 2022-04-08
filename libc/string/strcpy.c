#include <string.h>

char *strcpy(char *dest, const char* src) {
    unsigned char *d = (unsigned char *) dest;
    const unsigned char *s = (const unsigned char *) src;

    size_t len = 0;
    while(s[len]) {
        d[len] = s[len];
        len++;
    }

    return dest;
}