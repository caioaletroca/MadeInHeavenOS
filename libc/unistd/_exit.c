#include <unistd.h>
#include <syscall.h>

_Noreturn void _exit(int status)
{
    __syscall(SYS_EXIT, status);
    __builtin_unreachable();
}