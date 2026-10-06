#include <string.h>

__attribute__((optimize("no-tree-loop-distribute-patterns")))
size_t
strlcpy(char *dest, const char *src, size_t size)
{
    size_t length = strlen(src);

    if (size > 0)
    {
        size_t copy_length = (length >= size) ? size - 1 : length;
        memcpy(dest, src, copy_length);
        dest[copy_length] = '\0';
    }

    return length;
}