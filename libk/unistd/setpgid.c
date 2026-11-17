#include <unistd.h>
#include <syscall.h>

int setpgid(pid_t pid, pid_t group)
{
    return syscall(SYS_SETPGID, pid, group);
}