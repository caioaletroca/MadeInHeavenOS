#include <FILE.h>
#include <stdio.h>

int fputc(int c, FILE *stream)
{
    unsigned char byte = (unsigned char)c;
    return __fwritex(&byte, 1, stream) == 1 ? byte : EOF;
}