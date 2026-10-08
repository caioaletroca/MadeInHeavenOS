#include <FILE.h>
#include <stdio.h>

void __stdio_exit(void)
{
    fflush(NULL);
}