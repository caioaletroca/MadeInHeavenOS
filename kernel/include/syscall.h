#ifndef _SYSCALL_H
#define _SYSCALL_H

// System call numbers for the kernel.
#define SYS_EXIT 0
#define SYS_WRITE 1
#define SYS_READ 2
#define SYS_FORK 3
#define SYS_EXEC 4
#define SYS_WAIT 5
#define SYS_GETPID 6
#define SYS_KILL 7
#define SYS_YIELD 8
#define SYS_SLEEP 9
#define SYS_CLOSE 10

#define ENOEXEC 8
#define EBADF 9
#define ENOMEM 12
#define EFAULT 14
#define ENOSYS 38
#define EMFILE 24

#ifndef __ASSEMBLER__

/**
 * @brief Run a syscall on behalf of the current thread.
 *
 * @param number Syscall number (SYS_*).
 * @return Result for the caller, or -errno.
 */
long syscall_dispatch(unsigned long number, unsigned long arg0, unsigned long arg1, unsigned long arg2, unsigned long arg3, unsigned long arg4, unsigned long arg5);

#endif // __ASSEMBLER__

#endif // _SYSCALL_H