#ifndef _SIGNAL_H_
#define _SIGNAL_H_

#include <sys/cdefs.h>
#include <sys/types.h>
#include <mihos/signal.h>

typedef int sig_atomic_t;

__BEGIN_DECLS

/**
 * @brief Sends a signal to a process.
 * pid 0 and negative pids name process groups or everyone, which do not exist yet: no such process.
 * Signal 0 sends nothing and only checks that the process exists.
 *
 * @param pid The process ID to which the signal will be sent.
 * @param signal The signal number to send.
 * @return 0 on success, or -1 on error with errno set appropriately.
 */
int kill(pid_t pid, int signal);

/**
 * @brief Sends a signal to the calling process.
 *
 * @param signal The signal number to send.
 * @return 0 on success, or -1 on error with errno set appropriately.
 */
int raise(int signal);

/**
 * @brief Examines or changes the action taken by a process on receipt of a specific signal.
 *
 * @param signal The signal number.
 * @param action The new action for the signal.
 * @param old The old action for the signal.
 * @return 0 on success, or -1 on error with errno set appropriately.
 */
int sigaction(int signal, const struct sigaction *action, struct sigaction *old);

/**
 * @brief Examines or changes the signal mask of the calling process.
 *
 * @param how The how argument specifies the behavior of the call.
 * @param set The new signal mask.
 * @param old The old signal mask.
 * @return 0 on success, or -1 on error with errno set appropriately.
 */
int sigprocmask(int how, const sigset_t *set, sigset_t *old);

/**
 * @brief Initializes the signal set to be empty.
 *
 * @param set The signal set to initialize.
 * @return 0 on success, or -1 on error with errno set appropriately.
 */
int sigemptyset(sigset_t *set);

/**
 * @brief Initializes the signal set to contain all signals.
 *
 * @param set The signal set to initialize.
 * @return 0 on success, or -1 on error with errno set appropriately.
 */
int sigfillset(sigset_t *set);

/**
 * @brief Adds a signal to the signal set.
 *
 * @param set The signal set.
 * @param signal The signal number to add.
 * @return 0 on success, or -1 on error with errno set appropriately.
 */
int sigaddset(sigset_t *set, int signal);

/**
 * @brief Removes a signal from the signal set.
 *
 * @param set The signal set.
 * @param signal The signal number to remove.
 * @return 0 on success, or -1 on error with errno set appropriately.
 */
int sigdelset(sigset_t *set, int signal);

/**
 * @brief Checks if a signal is a member of the signal set.
 *
 * @param set The signal set.
 * @param signal The signal number to check.
 * @return 1 if the signal is a member, 0 if not, or -1 on error with errno set appropriately.
 */
int sigismember(const sigset_t *set, int signal);

__END_DECLS

#endif // _SIGNAL_H_