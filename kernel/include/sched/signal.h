#ifndef _SCHED_SIGNAL_H_
#define _SCHED_SIGNAL_H_

#include <stdbool.h>
#include <sched/process.h>

/**
 * @brief Records a signal for a process and wakes it from an interruptible sleep.
 *
 * Does nothing for init, for a process that already exited, and for signals
 * whose default action is to do nothing.
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