#include <unistd.h>
#include <time.h>
#include <errno.h>

unsigned int sleep(unsigned int seconds)
{
    struct timespec request = {
        .tv_sec = seconds,
        .tv_nsec = 0,
    };
    struct timespec remaining;

    if (nanosleep(&request, &remaining) == 0)
        return 0;

    return (unsigned int)remaining.tv_sec + (remaining.tv_nsec > 0);
}