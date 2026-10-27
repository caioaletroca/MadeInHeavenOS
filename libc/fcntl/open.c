#include <fcntl.h>
#include <stdarg.h>
#include <syscall.h>

int open(const char *path, int flags, ...)
{
    // The mode is only passed with O_CREAT: reading it otherwise would read garbage
    mode_t mode = 0;
    if (flags & O_CREAT)
    {
        va_list args;
        va_start(args, flags);
        mode = va_arg(args, mode_t);
        va_end(args);
    }

    return syscall(SYS_OPEN, path, flags, mode);
}