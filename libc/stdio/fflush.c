
#include <FILE.h>
#include <stdio.h>

int fflush(FILE *stream)
{
    if (stream != NULL)
        return __fflush_one(stream);

    int result = 0;
    for (FILE *f = __stdio_head; f != NULL; f = f->next)
    {
        if (__fflush_one(f) != 0)
            result = EOF;
    }

    return result;
}