#include <FILE.h>

// Every stream as fclose leaves it (POSIX exit): output written, input read ahead given back
void __stdio_exit(void)
{
    for (FILE *f = __stdio_head; f != NULL; f = f->next)
        __stream_sync(f);
}