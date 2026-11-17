#include <string.h>

char *strpbrk(const char *s, const char *accept)
{
    const char *p = s + strcspn(s, accept);
    return *p != '\0' ? (char *)p : NULL;
}