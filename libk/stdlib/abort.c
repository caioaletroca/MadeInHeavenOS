#include <stdlib.h>
#include <signal.h>
#include <unistd.h>

__attribute__((__noreturn__)) void abort(void)
{
    // A handler gets its chance first; blocked waits for the unblock below
    raise(SIGABRT);

    struct sigaction action = {0};
    sigaction(SIGABRT, &action, NULL);

    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGABRT);
    sigprocmask(SIG_UNBLOCK, &set, NULL);

    raise(SIGABRT);
    _exit(134);
}