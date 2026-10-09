#include <string.h>

size_t strspn(const char *s, const char *accept)
{
    const char *p = s;
    while (*p)
    {
        const char *a = accept;
        while (*a)
        {
            if (*p == *a)
                break;
            a++;
        }
        if (!*a)
            break;
        p++;
    }
    return p - s;
}