#include <unistd.h>
#include <syscall.h>

__attribute__((__noreturn__)) void _exit(int status)
{
    __syscall(SYS_EXIT, status);
    __builtin_unreachable();
}