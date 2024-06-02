#include "FILE.h"
#include <stdio.h>

int fputc(int c, FILE *stream) {
    *stream->pointer++ = c;
}