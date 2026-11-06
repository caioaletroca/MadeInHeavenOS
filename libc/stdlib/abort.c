#include <stdlib.h>
#include <signal.h>
#include <unistd.h>

__attribute__((__noreturn__)) void abort(void)
{
    raise(SIGABRT);
    _exit(134);
}