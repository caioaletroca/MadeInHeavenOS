#include <string.h>

size_t strcspn(const char *s, const char *reject)
{
    const char *p = s;
    while (*p)
    {
        const char *r = reject;
        while (*r)
        {
            if (*p == *r)
                return p - s;
            r++;
        }
        p++;
    }
    return p - s;
}