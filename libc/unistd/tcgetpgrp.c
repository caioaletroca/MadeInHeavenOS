#include <unistd.h>
#include <sys/ioctl.h>

pid_t tcgetpgrp(int fd)
{
    pid_t group;
    if (ioctl(fd, TIOCGPGRP, &group) < 0)
        return -1;

    return group;
}