#include <sys/ioctl.h>
#include <stdarg.h>
#include <syscall.h>

_Static_assert(sizeof(struct winsize) == sizeof(struct mihos_winsize), "struct winsize must match struct mihos_winsize");

int ioctl(int fd, unsigned long request, ...)
{
    va_list ap;
    va_start(ap, request);
    void *arg = va_arg(ap, void *);
    va_end(ap);

    return syscall(SYS_IOCTL, fd, request, arg);
}