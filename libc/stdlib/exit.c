#include <stdlib.h>
#include <unistd.h>
#include <FILE.h>

__attribute__((__noreturn__)) void exit(int status)
{
    __stdio_exit();
    _exit(status);
}