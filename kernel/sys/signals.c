#include <mihos/signal.h>
#include <sched/signal.h>
#include "internal.h"

/**
 * kill(pid, signal): sends a signal to a process. Signal 0 sends nothing and
 * only checks that the process exists. pid 0 and negative pids name process
 * groups or everyone, which do not exist yet: no such process.
 */
long sys_kill(long pid, int signal)
{
    if (signal < 0 || signal >= NSIG)
        return -EINVAL;

    if (pid <= 0)
        return -ESRCH;

    process_t *p = process_find((uint32_t)pid);
    if (p == NULL)
        return -ESRCH;

    if (signal != 0)
        signal_send(p, signal);

    process_release(p);
    return 0;
}