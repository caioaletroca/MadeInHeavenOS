#ifndef _INTERNAL_SYSCALL_H_
#define _INTERNAL_SYSCALL_H_

#include "syscall_arch.h"
#include <mihos/syscall.h>

#define __SYSCALL_NARGS_X(a, b, c, d, e, f, g, count, ...) count
#define __SYSCALL_NARGS(...) __SYSCALL_NARGS_X(__VA_ARGS__, 6, 5, 4, 3, 2, 1, 0, )

#define __SYSCALL_CONCAT_X(a, b) a##b
#define __SYSCALL_CONCAT(a, b) __SYSCALL_CONCAT_X(a, b)

/**
 * Perform a system call with a variable number of arguments.
 *
 * @param ... The system call number followed by its arguments.
 * @return The return value of the system call.
 */
#define __syscall(...) __SYSCALL_CONCAT(__syscall, __SYSCALL_NARGS(__VA_ARGS__))(__VA_ARGS__)

/**
 * Perform a system call and return its result.
 *
 * @param ... The system call number followed by its arguments.
 * @return The return value of the system call.
 */
#define syscall(...) __syscall_ret(__syscall(__VA_ARGS__))

/**
 * Return the result of a system call.
 *
 * @param result The raw result of the system call.
 * @return The processed return value of the system call.
 */
long __syscall_ret(unsigned long result);

#endif /* _INTERNAL_SYSCALL_H_ */