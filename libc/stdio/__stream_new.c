#include <FILE.h>
#include <stdlib.h>

FILE *__stream_new(int fd, unsigned int flags)
{
    FILE *f = malloc(sizeof(FILE) + BUFSIZ);
    if (f == NULL)
        return NULL;

    *f = (FILE){
        .fd = fd,
        .flags = flags | F_LINEBUF | F_TTYCHECK,
        .mode = MODE_NONE,
        .buffer = (unsigned char *)(f + 1),
        .own_buffer = (unsigned char *)(f + 1),
        .size = BUFSIZ,
        .ungot = EOF,
        .next = __stdio_head};

    __stdio_head = f;
    return f;
}