#include <signal.h>
#include <errno.h>

/**
 * Checks if a signal number is valid.
 * Returns 1 if valid, 0 otherwise and sets errno to EINVAL.
 */
static int signal_valid(int signal)
{
    if (signal > 0 && signal < NSIG)
        return 1;

    errno = EINVAL;
    return 0;
}

int sigemptyset(sigset_t *set)
{
    *set = 0;
    return 0;
}

int sigfillset(sigset_t *set)
{
    *set = ~0u;
    return 0;
}

int sigaddset(sigset_t *set, int signal)
{
    if (!signal_valid(signal))
        return -1;

    *set |= 1u << signal;
    return 0;
}

int sigdelset(sigset_t *set, int signal)
{
    if (!signal_valid(signal))
        return -1;

    *set &= ~(1u << signal);
    return 0;
}

int sigismember(const sigset_t *set, int signal)
{
    if (!signal_valid(signal))
        return -1;

    return (*set >> signal) & 1;
}