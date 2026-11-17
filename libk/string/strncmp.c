#include <string.h>

int strncmp(const char *s1, const char *s2, size_t n)
{
    for (size_t i = 0; i < n; i++)
    {
        // Compared as unsigned char, as the standard requires
        unsigned char a = s1[i], b = s2[i];
        if (a != b)
            return a - b;
        if (a == '\0')
            return 0; // both strings ended together
    }
    return 0;
}