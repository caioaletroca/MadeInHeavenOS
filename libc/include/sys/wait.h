#ifndef _SYS_WAIT_H_
#define _SYS_WAIT_H_

#include <sys/cdefs.h>
#include <sys/types.h>
#include <mihos/wait.h>

__BEGIN_DECLS

// Status from waitpid: exit value in bits 8-15, the terminating signal in bits 0-6,
// or 0x7F in the low byte and the stopping signal in bits 8-15
#define WEXITSTATUS(status) (((status) >> 8) & 0xFF)
#define WTERMSIG(status) ((status) & 0x7F)
#define WSTOPSIG(status) WEXITSTATUS(status)
#define WIFEXITED(status) (WTERMSIG(status) == 0)
#define WIFSTOPPED(status) (((status) & 0xFF) == 0x7F)
#define WIFSIGNALED(status) (WTERMSIG(status) != 0 && !WIFSTOPPED(status))

/**
 * Wait for a child process to change state.
 *
 * @param pid The PID of the child process to wait for.
 * @param status Pointer to an integer to store the child's exit status.
 * @param options Options for waiting.
 * @return The PID of the child process that changed state, or -1 on error.
 */
pid_t waitpid(pid_t pid, int *status, int options);

/**
 * Wait for any child process to change state.
 *
 * @param status Pointer to an integer to store the child's exit status.
 * @return The PID of the child process that changed state, or -1 on error.
 */
pid_t wait(int *status);

__END_DECLS

#endif /* _SYS_WAIT_H_ */