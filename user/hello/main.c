#include <stddef.h>
#include <syscall.h>

/*
 * First ELF program: prints a line and exits with 42, so the exit status
 * the kernel reports visibly comes from the program. No libc yet (4f-4).
 *
 * The status also checks the loader: 42 comes from .data (copied from the
 * file) plus a .bss variable that must be zero (only in memory). Missing
 * data gives 0, an unzeroed .bss gives anything but 42.
 */

static long syscall3(long number, long arg0, long arg1, long arg2)
{
    long result;

    __asm__ __volatile__("int $0x80"
                         : "=a"(result)
                         : "a"(number), "D"(arg0), "S"(arg1), "d"(arg2)
                         : "memory");
    return result;
}

static const char message[] = "Hello from an ELF program!\n";

// volatile: keep the compiler from folding them into a constant 42
static volatile int status = 42; // .data
static volatile int zero;        // .bss

int main(void)
{
    syscall3(SYS_WRITE, 1, (long)message, sizeof(message) - 1);
    return status + zero;
}
