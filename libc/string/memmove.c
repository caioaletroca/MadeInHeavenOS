#include <string.h>

void *memmove(void *str1, const void *str2, size_t n)
{
    unsigned char *d = (unsigned char *)str1;
    const unsigned char *s = (const unsigned char *)str2;

    if (d < s)
    {
        // To the left: front to back never overwrites a byte not yet read
        for (size_t i = 0; i < n; i++)
            d[i] = s[i];
    }
    else if (d > s)
    {
        // To the right: back to front, for the same reason
        for (size_t i = n; i > 0; i--)
            d[i - 1] = s[i - 1];
    }

    return str1;
}