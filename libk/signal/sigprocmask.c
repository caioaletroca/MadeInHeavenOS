#include <signal.h>
#include <syscall.h>

int sigprocmask(int how, const sigset_t *set, sigset_t *old)
{
    return syscall(SYS_SIGPROCMASK, how, (long)set, (long)old);
}