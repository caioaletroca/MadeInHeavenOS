#ifndef _SYSCALL_ARCH_H_
#define _SYSCALL_ARCH_H_

/**
 * Perform a system call with no arguments.
 *
 * @param n The system call number.
 * @return The return value of the system call.
 */
static inline long __syscall0(long n)
{
    long ret;
    __asm__ __volatile__("int $0x80" : "=a"(ret) : "a"(n) : "memory");
    return ret;
}

/**
 * Perform a system call with one argument.
 *
 * @param n The system call number.
 * @param a The first argument.
 * @return The return value of the system call.
 */
static inline long __syscall1(long n, long a)
{
    long ret;
    __asm__ __volatile__("int $0x80" : "=a"(ret) : "a"(n), "D"(a) : "memory");
    return ret;
}

/**
 * Perform a system call with two arguments.
 *
 * @param n The system call number.
 * @param a The first argument.
 * @param b The second argument.
 * @return The return value of the system call.
 */
static inline long __syscall2(long n, long a, long b)
{
    long ret;
    __asm__ __volatile__("int $0x80" : "=a"(ret) : "a"(n), "D"(a), "S"(b) : "memory");
    return ret;
}

/**
 * Perform a system call with three arguments.
 *
 * @param n The system call number.
 * @param a The first argument.
 * @param b The second argument.
 * @param c The third argument.
 * @return The return value of the system call.
 */
static inline long __syscall3(long n, long a, long b, long c)
{
    long ret;
    __asm__ __volatile__("int $0x80" : "=a"(ret) : "a"(n), "D"(a), "S"(b), "d"(c) : "memory");
    return ret;
}

/**
 * Perform a system call with four arguments.
 *
 * @param n The system call number.
 * @param a The first argument.
 * @param b The second argument.
 * @param c The third argument.
 * @param d The fourth argument.
 * @return The return value of the system call.
 */
static inline long __syscall4(long n, long a, long b, long c, long d)
{
    register long r10 __asm__("r10") = d;
    long ret;
    __asm__ __volatile__("int $0x80" : "=a"(ret) : "a"(n), "D"(a), "S"(b), "d"(c), "r"(r10) : "memory");
    return ret;
}

/**
 * Perform a system call with five arguments.
 *
 * @param n The system call number.
 * @param a The first argument.
 * @param b The second argument.
 * @param c The third argument.
 * @param d The fourth argument.
 * @param e The fifth argument.
 * @return The return value of the system call.
 */
static inline long __syscall5(long n, long a, long b, long c, long d, long e)
{
    register long r10 __asm__("r10") = d;
    register long r8 __asm__("r8") = e;
    long ret;
    __asm__ __volatile__("int $0x80"
                         : "=a"(ret)
                         : "a"(n), "D"(a), "S"(b), "d"(c), "r"(r10), "r"(r8)
                         : "memory");
    return ret;
}

/**
 * Perform a system call with six arguments.
 *
 * @param n The system call number.
 * @param a The first argument.
 * @param b The second argument.
 * @param c The third argument.
 * @param d The fourth argument.
 * @param e The fifth argument.
 * @param f The sixth argument.
 * @return The return value of the system call.
 */
static inline long __syscall6(long n, long a, long b, long c, long d, long e, long f)
{
    register long r10 __asm__("r10") = d;
    register long r8 __asm__("r8") = e;
    register long r9 __asm__("r9") = f;
    long ret;
    __asm__ __volatile__("int $0x80"
                         : "=a"(ret)
                         : "a"(n), "D"(a), "S"(b), "d"(c), "r"(r10), "r"(r8), "r"(r9)
                         : "memory");
    return ret;
}

/**
 * Convenience macros for system calls with type casting.
 */
#define __syscall1(n, a) (__syscall1)((n), (long)(a))
#define __syscall2(n, a, b) (__syscall2)((n), (long)(a), (long)(b))
#define __syscall3(n, a, b, c) (__syscall3)((n), (long)(a), (long)(b), (long)(c))
#define __syscall4(n, a, b, c, d) (__syscall4)((n), (long)(a), (long)(b), (long)(c), (long)(d))
#define __syscall5(n, a, b, c, d, e) (__syscall5)((n), (long)(a), (long)(b), (long)(c), (long)(d), (long)(e))
#define __syscall6(n, a, b, c, d, e, f) (__syscall6)((n), (long)(a), (long)(b), (long)(c), (long)(d), (long)(e), (long)(f))

#endif /* _SYSCALL_ARCH_H_ */