#include <FILE.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>

int vfprintf(FILE *stream, const char *format, va_list args)
{
    char small[BUFSIZ];
    char *text = small;

    va_list again;
    va_copy(again, args);

    int n = vsnprintf(small, sizeof(small), format, args);

    if (n >= 0 && (size_t)n >= sizeof(small))
    {
        text = malloc((size_t)n + 1);
        if (text == NULL)
        {
            va_end(again);
            stream->flags |= F_ERR;
            return -1;
        }
        vsnprintf(text, (size_t)n + 1, format, again);
    }
    va_end(again);

    if (n < 0)
        return n;

    int result = __fwritex((const unsigned char *)text, (size_t)n, stream) == (size_t)n ? n : -1;
    if (text != small)
        free(text);

    return result;
}