#ifndef _SIGNAL_H_
#define _SIGNAL_H_

#include <sys/cdefs.h>
#include <sys/types.h>
#include <mihos/signal.h>

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

__END_DECLS

#endif // _SIGNAL_H_