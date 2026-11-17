#include <termios.h>
#include <sys/ioctl.h>

int tcgetattr(int fd, struct termios *settings)
{
    return ioctl(fd, TCGETS, settings);
}