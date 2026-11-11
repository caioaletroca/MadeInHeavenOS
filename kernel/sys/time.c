#include "internal.h"
#include <driver/clock.h>
#include <driver/timer.h>
#include <sched/wait.h>
#include <sched/process.h>
#include <sched/scheduler.h>
#include <mihos/time.h>
#include <stdint.h>

// ---- Clocks and sleeping: clock_gettime, nanosleep ----

/**
 * @brief Convert nanoseconds to a timespec structure.
 *
 * @param nanoseconds The time in nanoseconds.
 * @return A struct mihos_timespec representing the time.
 */
static struct mihos_timespec timespec_from(int64_t nanoseconds)
{
    return (struct mihos_timespec){
        .tv_sec = nanoseconds / NANOSECONDS_PER_SECOND,
        .tv_nsec = nanoseconds % NANOSECONDS_PER_SECOND,
    };
}

/**
 * sys_clock_gettime(): reads a clock.
 *
 * @param clock MIHOS_CLOCK_REALTIME or MIHOS_CLOCK_MONOTONIC.
 * @param time User pointer to a struct mihos_timespec.
 * @return 0, -EINVAL for another clock, -EFAULT for a bad pointer.
 */
long sys_clock_gettime(int clock, uintptr_t time)
{
    int64_t now;
    if (clock == MIHOS_CLOCK_REALTIME)
        now = clock_realtime();
    else if (clock == MIHOS_CLOCK_MONOTONIC)
        now = clock_monotonic();
    else if (clock == MIHOS_CLOCK_PROCESS_CPUTIME_ID)
        now = (int64_t)timer_ticks_to_nanoseconds(scheduler_current()->process->cpu_ticks);
    else
        return -EINVAL;

    struct mihos_timespec value = timespec_from(now);
    if (address_space_write(syscall_space(), time, &value, sizeof(value)) != 0)
        return -EFAULT;

    return 0;
}

/**
 * sys_nanosleep(): sleeps at least the time asked, unless a signal comes first.
 *
 * @param request User pointer to how long to sleep.
 * @param remaining User pointer (or 0) that receives the time left when a signal ends it.
 * @return 0, -EINTR (remaining written), -EINVAL for a negative or malformed time, -EFAULT.
 */
long sys_nanosleep(uintptr_t request, uintptr_t remaining)
{
    scheduler_current()->syscall_number = SYSCALL_NOT_RESTARTABLE;

    struct mihos_timespec wanted;
    if (address_space_read(syscall_space(), request, &wanted, sizeof(wanted)) != 0)
        return -EFAULT;

    if (wanted.tv_sec < 0 || wanted.tv_nsec < 0 || wanted.tv_nsec >= NANOSECONDS_PER_SECOND)
        return -EINVAL;

    int64_t length = wanted.tv_sec >= INT64_MAX / NANOSECONDS_PER_SECOND ? INT64_MAX : wanted.tv_sec * NANOSECONDS_PER_SECOND + wanted.tv_nsec;
    int64_t start = clock_monotonic();
    int64_t deadline = length > INT64_MAX - start ? INT64_MAX : start + length;

    // Nobody wakes this queue: only the time limit or a signal ends the sleep
    wait_queue_t nobody;
    wait_queue_init(&nobody);
    spinlock_t lock = SPINLOCK_INIT;

    int result;
    scoped_guard(spinlock, &lock)
    {
        result = wait_queue_sleep_interruptible_timeout(&nobody, &lock, timer_ticks_for((uint64_t)length));
    }

    if (result != -EINTR)
        return 0;

    if (remaining != 0)
    {
        int64_t left = deadline - clock_monotonic();
        struct mihos_timespec value = timespec_from(left > 0 ? left : 0);
        if (address_space_write(syscall_space(), remaining, &value, sizeof(value)) != 0)
            return -EFAULT;
    }

    return -EINTR;
}