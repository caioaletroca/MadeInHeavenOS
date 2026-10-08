#include <FILE.h>
#include <stdio.h>
#include <string.h>

int fputs(const char *s, FILE *stream)
{
    // Compare with the length: __fwritex also returns 0 for an empty string
    size_t length = strlen(s);
    return __fwritex((const unsigned char *)s, length, stream) == length ? 0 : EOF;
}