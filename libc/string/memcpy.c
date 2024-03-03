#include <string.h>

__attribute__((optimize("no-tree-loop-distribute-patterns")))
void *memcpy(void *dest, const void * src, size_t n) {
    unsigned char *d = (unsigned char *) dest;
    const unsigned char *s = (const unsigned char *) src;
    for(size_t i = 0; i < n; i++)
        d[i] = s[i];
    return dest;
}