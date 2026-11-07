#include <unistd.h>
#include <sys/ioctl.h>

int tcsetpgrp(int fd, pid_t group)
{
    return ioctl(fd, TIOCSPGRP, &group);
}