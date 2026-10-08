#include <FILE.h>
#include <stdio.h>
#include <stdarg.h>

int vfprintf(FILE *stream, const char *format, va_list args)
{
    char buf[BUFSIZ];
    int n = vsnprintf(buf, BUFSIZ, format, args);

    if (n < 0)
        return n;
    if ((size_t)n >= sizeof(buf))
        n = sizeof(buf) - 1;

    return __fwritex((const unsigned char *)buf, n, stream) == (size_t)n ? n : -1;
}