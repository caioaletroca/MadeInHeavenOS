#include <sched/signal.h>
#include <arch/signal.h>
#include <mihos/signal.h>
#include <sched/process.h>
#include <sched/scheduler.h>
#include <sched/thread.h>

#define SIGNAL_STOPS ((1u << SIGSTOP) | (1u << SIGTSTP) | (1u << SIGTTIN) | (1u << SIGTTOU))

static bool signal_ignored_by_default(int signal)
{
    switch (signal)
    {
    case SIGCHLD:
    case SIGCONT:
    case SIGURG:
    case SIGWINCH:
        return true;
    default:
        return false;
    }
}

/**
 * @brief Whether a signal does nothing for a given process.
 *
 * @param p The process.
 * @param signal The signal.
 * @return true if the signal is ignored or has the default action and is ignored by default.
 */
static bool signal_does_nothing(process_t *p, int signal)
{
    void (*handler)(int) = p->signal_actions[signal].sa_handler;
    return handler == SIG_IGN || (handler == SIG_DFL && signal_ignored_by_default(signal));
}

void signal_send(process_t *p, int signal)
{
    if (p == NULL)
        return;

    guard(spinlock, &p->lock);

    // p->thread is gone once the process exited: state is set under this lock
    if (p->state == PROCESS_EXITED)
        return;

    // init takes only what it has a handler for: its exit panics the kernel
    if (p->pid == PROCESS_INIT_PID && p->signal_actions[signal].sa_handler == SIG_DFL)
        return;

    // These two act at once, whatever the signal's action: a stopped process runs nothing else
    if (signal == SIGCONT || signal == SIGKILL)
    {
        p->stopped = false;
        wait_queue_wake_all(&p->resume_waiters);
    }

    uint32_t bit = 1u << signal;

    if (signal == SIGCONT)
        p->signals_pending &= ~SIGNAL_STOPS;
    else if ((SIGNAL_STOPS & bit) != 0)
        p->signals_pending &= ~(1u << SIGCONT);

    // A blocked signal stays: the action may change before it is unblocked
    if ((p->signals_blocked & bit) == 0 && signal_does_nothing(p, signal))
        return;

    p->signals_pending |= bit;

    // NULL while the process is being created
    if ((p->signals_blocked & bit) == 0 && p->thread != NULL && p->thread->interruptible)
        scheduler_wake(p->thread);
}

void signal_force(int signal)
{
    process_t *p = scheduler_current()->process;
    uint32_t bit = 1u << signal;

    guard(spinlock, &p->lock);

    if ((p->signals_blocked & bit) != 0 ||
        p->signal_actions[signal].sa_handler == SIG_IGN)
    {
        p->signals_blocked &= ~bit;
        p->signal_actions[signal] = (struct sigaction){0};
    }

    p->signals_pending |= bit;
}

bool signal_pending(process_t *p)
{
    return p != NULL && (p->signals_pending & ~p->signals_blocked) != 0;
}

bool signal_refused(process_t *p, int signal)
{
    return (p->signals_blocked & (1u << signal)) != 0 || p->signal_actions[signal].sa_handler == SIG_IGN;
}

void signal_action_set(process_t *p, int signal, const struct sigaction *action)
{
    guard(spinlock, &p->lock);

    p->signal_actions[signal] = *action;
    p->signal_actions[signal].sa_mask &= ~SIGNAL_UNBLOCKABLE;

    if (signal_does_nothing(p, signal))
        p->signals_pending &= ~(1u << signal);
}

void signal_inherit(process_t *child, process_t *parent)
{
    for (int signal = 0; signal < NSIG; signal++)
        child->signal_actions[signal] = parent->signal_actions[signal];

    child->signals_blocked = parent->signals_blocked;
}

void signal_exec_reset(process_t *p)
{
    for (int signal = 0; signal < NSIG; signal++)
        if (p->signal_actions[signal].sa_handler != SIG_IGN)
            p->signal_actions[signal] = (struct sigaction){0};
}

void signal_deliver(void)
{
    thread_t *thread = scheduler_current();
    process_t *p = thread->process;

    while (signal_pending(p))
    {
        int signal = __builtin_ctz(p->signals_pending & ~p->signals_blocked);
        struct sigaction action = p->signal_actions[signal];
        p->signals_pending &= ~(1u << signal);

        if (signal_does_nothing(p, signal))
            continue;

        if (action.sa_handler == SIG_DFL)
        {
            if ((SIGNAL_STOPS & (1u << signal)) == 0)
                process_exit_signal(signal);

            // Nothing of the program runs, so it must not see EINTR
            arch_syscall_restart(thread->stack, thread->stack_size, thread->syscall_number);
            process_stop(signal);
            continue;
        }

        if ((action.sa_flags & SA_RESTART) != 0)
            arch_syscall_restart(thread->stack, thread->stack_size, thread->syscall_number);

        if (arch_signal_frame_push(
                thread->stack,
                thread->stack_size,
                p->space,
                signal,
                (uintptr_t)action.sa_handler,
                (uintptr_t)action.sa_restorer,
                p->signals_blocked) != 0)
            process_exit_signal(SIGSEGV);

        // Its own signal too, so a handler is not entered again while it runs
        sigset_t block = action.sa_mask;
        if ((action.sa_flags & SA_NODEFER) == 0)
            block |= 1u << signal;

        p->signals_blocked |= block & ~SIGNAL_UNBLOCKABLE;

        if ((action.sa_flags & SA_RESETHAND) != 0)
            p->signal_actions[signal] = (struct sigaction){0};

        return;
    }
}