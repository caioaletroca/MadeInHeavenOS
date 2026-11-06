#include <unistd.h>
#include <syscall.h>

int pause(void)
{
    return syscall(SYS_PAUSE);
}