#include <stdio.h>

void setbuf(FILE *stream, char *buffer)
{
    // C: a buffer (of at least BUFSIZ bytes) makes the stream fully buffered, NULL unbuffered
    (void)setvbuf(stream, buffer, buffer != NULL ? _IOFBF : _IONBF, BUFSIZ);
}