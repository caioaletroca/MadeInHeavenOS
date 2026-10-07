#include <stdlib.h>
#include <unistd.h>

__attribute__((__noreturn__)) void exit(int status)
{
    // TODO: run atexit handlers and flush stdio before _exit
    _exit(status);
}