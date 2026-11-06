#include <mihos/signal.h>
#include <sched/signal.h>
#include <arch/signal.h>
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

/**
 * sys_sigaction(signal, action, old): examines or changes the action taken by a process on receipt of a specific signal.
 */
long sys_sigaction(int signal, uintptr_t action, uintptr_t old)
{
    process_t *p = scheduler_current()->process;

    if (signal <= 0 || signal >= NSIG)
        return -EINVAL;

    if (action != 0 && (SIGNAL_UNBLOCKABLE & (1u << signal)) != 0)
        return -EINVAL;

    struct sigaction incoming;
    if (action != 0 && address_space_read(syscall_space(), action, &incoming, sizeof(incoming)) != 0)
        return -EFAULT;

    if (old != 0 && address_space_write(syscall_space(), old, &p->signal_actions[signal], sizeof(struct sigaction)) != 0)
        return -EFAULT;

    if (action != 0)
        signal_action_set(p, signal, &incoming);

    return 0;
}

/**
 * sys_sigprocmask(how, set, old): examines or changes the set of blocked signals for the current process.
 */
long sys_sigprocmask(int how, uintptr_t set, uintptr_t old)
{
    process_t *p = scheduler_current()->process;
    sigset_t blocked = p->signals_blocked;

    if (set != 0)
    {
        sigset_t incoming;
        if (address_space_read(syscall_space(), set, &incoming, sizeof(incoming)) != 0)
            return -EFAULT;

        switch (how)
        {
        case SIG_BLOCK:
            blocked |= incoming;
            break;
        case SIG_UNBLOCK:
            blocked &= ~incoming;
            break;
        case SIG_SETMASK:
            blocked = incoming;
            break;
        default:
            return -EINVAL;
        }
    }

    if (old != 0)
    {
        sigset_t current = p->signals_blocked;
        if (address_space_write(syscall_space(), old, &current, sizeof(current)) != 0)
            return -EFAULT;
    }

    p->signals_blocked = blocked & ~SIGNAL_UNBLOCKABLE;
    return 0;
}

/**
 * sys_sigreturn(): returns from a signal handler, restoring the previous signal mask and return value.
 */
long sys_sigreturn(void)
{
    thread_t *thread = scheduler_current();
    process_t *p = thread->process;
    uint32_t blocked;
    long result;

    if (arch_signal_frame_pop(
            thread->stack,
            thread->stack_size,
            p->space,
            &blocked,
            &result) != 0)
    {
        signal_force(SIGSEGV);
        return 0;
    }

    p->signals_blocked = blocked & ~SIGNAL_UNBLOCKABLE;
    return result;
}