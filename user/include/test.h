#ifndef _USER_TEST_H_
#define _USER_TEST_H_

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mihos/syscall.h>

/*
 * Checks for the programs in user/tests. Each test is one translation unit:
 * main returns test_status(), so the exit status is the number of failed
 * checks (0 = passed). The kernel's test runner compares it with the status
 * the module expects (see grub/grub.cfg).
 */

static int test_failures;

// Reports to stdout, so a test may close stderr
#define CHECK(expr)                                        \
    do                                                     \
    {                                                      \
        if (!(expr))                                       \
        {                                                  \
            test_failures++;                               \
            printf("FAIL line %d: %s\n", __LINE__, #expr); \
        }                                                  \
    } while (0)

static inline int test_status(void)
{
    return test_failures;
}

/*
 * Whether / is the installed system on the disk (writable ext2) rather than
 * the initrd (read-only, the archive's layout): the runner's TEST_ROOT. Checks
 * that only hold for one of the two boots ask this first.
 */
static inline int test_root_on_disk(void)
{
    const char *root = getenv("TEST_ROOT");
    return root != NULL && strcmp(root, "disk") == 0;
}

/*
 * A system call made directly, for checks of the kernel's own calls that no
 * libc wraps the same way (getdents, brk): the same on every libc. The way
 * libc returns: -1 with errno for an error (-errno from the kernel), else the
 * result as it is.
 */
static inline long test_syscall(long number, long first, long second, long third)
{
    long result;
    __asm__ __volatile__("int $0x80"
                         : "=a"(result)
                         : "a"(number), "D"(first), "S"(second), "d"(third)
                         : "rcx", "r11", "memory");
    if (result < 0 && result >= -4095)
    {
        errno = (int)-result;
        return -1;
    }
    return result;
}

// The kernel's getdents: struct mihos_dirent records (Linux's getdents64), which
// libcs wrap under different names (ours getdents, mlibc getdents64)
static inline long test_directory_records(int fd, void *buffer, size_t size)
{
    return test_syscall(SYS_GETDENTS, fd, (long)buffer, (long)size);
}

/*
 * The heap break, moved with the kernel's brk: mlibc's sbrk only reports it (its
 * malloc maps memory instead), so the tests of the break move it themselves.
 * Like Linux's, the call returns the break: the new one, or the old one when it
 * refuses. test_break_set is brk(), test_break_move is sbrk().
 */
static inline int test_break_set(void *address)
{
    if (test_syscall(SYS_BRK, (long)address, 0, 0) != (long)address)
    {
        errno = ENOMEM;
        return -1;
    }
    return 0;
}

static inline void *test_break_move(intptr_t increment)
{
    uintptr_t old = (uintptr_t)test_syscall(SYS_BRK, 0, 0, 0);
    if (increment == 0)
        return (void *)old;

    uintptr_t moved = old + (uintptr_t)increment;
    if ((increment > 0 && moved < old) || (increment < 0 && moved > old) || test_break_set((void *)moved) != 0)
    {
        errno = ENOMEM;
        return (void *)-1;
    }
    return (void *)old;
}

#endif /* _USER_TEST_H_ */
