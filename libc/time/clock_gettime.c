#include <time.h>
#include <syscall.h>

// struct timespec is passed to the kernel as is
_Static_assert(sizeof(struct timespec) == sizeof(struct mihos_timespec), "timespec layout");

int clock_gettime(clockid_t clock, struct timespec *time)
{
    return syscall(SYS_CLOCK_GETTIME, clock, time);
}