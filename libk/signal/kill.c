#include <signal.h>
#include <syscall.h>

int kill(pid_t pid, int signal)
{
    return syscall(SYS_KILL, pid, signal);
}