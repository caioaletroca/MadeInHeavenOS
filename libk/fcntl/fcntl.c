#include <fcntl.h>
#include <stdarg.h>
#include <syscall.h>

int fcntl(int fd, int command, ...)
{
    long arg = 0;
    if (command == F_SETFD)
    {
        va_list args;
        va_start(args, command);
        arg = va_arg(args, int);
        va_end(args);
    }

    return syscall(SYS_FCNTL, fd, command, arg);
}