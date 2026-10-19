#include <unistd.h>
#include <sys/ioctl.h>

int isatty(int fd)
{
    struct winsize ws;
    return ioctl(fd, TIOCGWINSZ, &ws) == 0;
}