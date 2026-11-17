#include <sys/reboot.h>
#include <unistd.h>
#include <syscall.h>

int reboot(int howto)
{
    return syscall(SYS_REBOOT, howto);
}