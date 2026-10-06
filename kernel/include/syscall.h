#ifndef _SYSCALL_H
#define _SYSCALL_H

#include <mihos/syscall.h>
#include <mihos/errno.h>

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