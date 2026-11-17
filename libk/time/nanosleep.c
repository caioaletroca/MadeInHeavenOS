#include <time.h>
#include <syscall.h>

int nanosleep(const struct timespec *request, struct timespec *remaining)
{
    return syscall(SYS_NANOSLEEP, request, remaining);
}