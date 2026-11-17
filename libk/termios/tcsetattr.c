#include <termios.h>
#include <sys/ioctl.h>
#include <errno.h>

int tcsetattr(int fd, int when, const struct termios *settings)
{
    switch (when)
    {
    case TCSANOW:
        return ioctl(fd, TCSETS, settings);
    case TCSADRAIN:
        return ioctl(fd, TCSETSW, settings);
    case TCSAFLUSH:
        return ioctl(fd, TCSETSF, settings);
    default:
        errno = EINVAL;
        return -1;
    }
}