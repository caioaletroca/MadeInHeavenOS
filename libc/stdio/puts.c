#include <stdio.h>

int puts(const char *s)
{
    if (fputs(s, stdout) == EOF || fputc('\n', stdout) == EOF)
        return EOF;

    return 0;
}