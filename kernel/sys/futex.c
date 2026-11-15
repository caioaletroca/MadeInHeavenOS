#include "internal.h"
#include <driver/timer.h>
#include <driver/clock.h>
#include <sched/futex.h>
#include <mihos/time.h>
#include <stdint.h>

// ---- Futexes: futex_wait, futex_wake ----

/**
 * futex_wait(address, expected, timeout): sleeps while the 32-bit word at address holds
 * expected, until futex_wake, the relative timeout (0: none) or a signal.
 */
long sys_futex_wait(uintptr_t address, uint32_t expected, uintptr_t timeout)
{
    // EINTR reaches the program, as nanosleep's: lock code checks the word again and retries
    scheduler_current()->syscall_number = SYSCALL_NOT_RESTARTABLE;

    uint64_t ticks = FUTEX_NO_TIME_LIMIT;
    if (timeout != 0)
    {
        struct mihos_timespec limit;
        if (address_space_read(syscall_space(), timeout, &limit, sizeof(limit)) != 0)
            return -EFAULT;
        if (limit.tv_sec < 0 || limit.tv_nsec < 0 || limit.tv_nsec >= NANOSECONDS_PER_SECOND)
            return -EINVAL;

        int64_t length = limit.tv_sec >= INT64_MAX / NANOSECONDS_PER_SECOND ? INT64_MAX : limit.tv_sec * NANOSECONDS_PER_SECOND + limit.tv_nsec;

        // A zero time limit only checks the word (timer_ticks_for would round it up to a tick)
        ticks = length == 0 ? 0 : timer_ticks_for((uint64_t)length);
    }

    return futex_wait(syscall_space(), address, expected, ticks);
}

/**
 * futex_wake(address, count): wakes up to count threads sleeping on the word at address;
 * returns how many.
 */
long sys_futex_wake(uintptr_t address, size_t count)
{
    if ((address & 3) != 0)
        return -EINVAL;

    return (long)futex_wake(syscall_space(), address, count);
}