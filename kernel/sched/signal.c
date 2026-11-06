#include <sched/signal.h>
#include <mihos/signal.h>
#include <sched/process.h>
#include <sched/scheduler.h>
#include <sched/thread.h>

// Stop signals do nothing until job control exists (C6)
static bool signal_ignored_by_default(int signal)
{
    switch (signal)
    {
    case SIGCHLD:
    case SIGCONT:
    case SIGURG:
    case SIGWINCH:
    case SIGSTOP:
    case SIGTSTP:
    case SIGTTIN:
    case SIGTTOU:
        return true;
    default:
        return false;
    }
}

void signal_send(process_t *p, int signal)
{
    // init has no handlers yet, and its exit panics the kernel
    if (p == NULL || p->pid == PROCESS_INIT_PID || signal_ignored_by_default(signal))
        return;

    guard(spinlock, &p->lock);

    // p->thread is gone once the process exited: state is set under this lock
    if (p->state == PROCESS_EXITED)
        return;

    p->signals_pending |= 1u << signal;

    // NULL while the process is being created
    if (p->thread != NULL && p->thread->interruptible)
        scheduler_wake(p->thread);
}

bool signal_pending(process_t *p)
{
    return p != NULL && p->signals_pending != 0;
}

void signal_deliver(void)
{
    process_t *p = scheduler_current()->process;
    if (!signal_pending(p))
        return;

    // Every signal left here ends the process: the lowest number goes first
    process_exit_signal(__builtin_ctz(p->signals_pending));
}