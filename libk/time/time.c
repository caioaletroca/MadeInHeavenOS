#include <time.h>
#include <stddef.h>

time_t time(time_t *t)
{
    struct timespec now;
    clock_gettime(CLOCK_REALTIME, &now);
    if (t != NULL)
        *t = now.tv_sec;
    return now.tv_sec;
}