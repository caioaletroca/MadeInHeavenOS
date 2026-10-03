#include <string.h>

__attribute__((optimize("no-tree-loop-distribute-patterns"))) int memcmp(const void *str1, const void *str2, size_t n)
{
    const unsigned char *a = (const unsigned char *)str1;
    const unsigned char *b = (const unsigned char *)str2;

    for (size_t i = 0; i < n; i++)
        if (a[i] != b[i])
            return a[i] - b[i];

    return 0;
}