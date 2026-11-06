#include <signal.h>
#include <stddef.h>
#include <syscall.h>

void __signal_restore(void);

int sigaction(int signal, const struct sigaction *action, struct sigaction *old)
{
    struct sigaction copy;

    if (action != NULL)
    {
        copy = *action;
        copy.sa_restorer = __signal_restore;
        action = &copy;
    }

    return syscall(SYS_SIGACTION, signal, (long)action, (long)old);
}