#ifndef _SCHED_SIGNAL_H_
#define _SCHED_SIGNAL_H_

#include <stdbool.h>
#include <sched/process.h>

#define SIGNAL_UNBLOCKABLE ((1u << SIGKILL) | (1u << SIGSTOP))

/**
 * @brief Sends a fault to the current process. A fault cannot be refused: the
 * instruction would fault forever, so blocked or ignored becomes the default action.
 *
 * @param signal The signal for the fault.
 */
void signal_force(int signal);

/**
 * @brief Replaces a signal's action. Ignoring it drops one already waiting.
 */
void signal_action_set(process_t *p, int signal, const struct sigaction *action);

/**
 * @brief What a new process takes from its parent: actions and mask, not pending signals.
 */
void signal_inherit(process_t *child, process_t *parent);

/**
 * @brief exec: handlers live in the old program, so they go back to the default.
 * Ignored signals, the mask and pending signals stay.
 */
void signal_exec_reset(process_t *p);

/**
 * @brief Records a signal for a process and wakes it from an interruptible sleep.
 *
 * Does nothing for init without a handler, for a process that already exited, and, unless it is blocked, for a signal
 * that would do nothing.
 *
 * @param p The target process.
 * @param signal The signal, 1..31.
 */
void signal_send(process_t *p, int signal);

/**
 * @brief Whether a process has a signal waiting to be acted on.
 *
 * @param p The process, or NULL for a kernel thread (which has none).
 * @return true if a signal is pending, false otherwise.
 */
bool signal_pending(process_t *p);

/**
 * @brief Acts on the current process's pending signal, if any. Call only on
 * the thread's own stack, on the way back to user mode. Does not return when
 * a signal ends the process.
 */
void signal_deliver(void);

#endif // _SCHED_SIGNAL_H_