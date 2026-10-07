#include <stdlib.h>
#include <unistd.h>

__attribute__((__noreturn__)) void abort(void)
{
    _exit(134);
}