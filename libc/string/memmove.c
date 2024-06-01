#include <string.h>

void *memmove(void *str1, const void *str2, size_t n) {
    unsigned char *d = (unsigned char *) str1;
    const unsigned char *s = (const unsigned char *) str2;

    // TODO: Fix this
    for(size_t i = 0; i < n; i++)
        d[i] = s[i];

    // unsigned char *buffer = malloc(n);

    // for(size_t i = 0; i < n; i++)
    //     buffer[i] = s[i];

    // for(size_t i = 0; i < n; i++)
    //     d[i] = buffer[i];

    return str1;
}