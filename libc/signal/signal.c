#include <signal.h>

void (*signal(int number, void (*handler)(int)))(int)
{
    struct sigaction action = {0};
    struct sigaction old;

    action.sa_handler = handler;
    action.sa_flags = SA_RESTART;

    if (sigaction(number, &action, &old) < 0)
        return SIG_ERR;

    return old.sa_handler;
}