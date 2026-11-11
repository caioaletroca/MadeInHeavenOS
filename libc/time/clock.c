#include <time.h>

clock_t clock(void)
{
    struct timespec used;
    if (clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &used) != 0)
        return (clock_t)-1;

    // Microseconds: a 64-bit clock_t lasts 292 000 years of CPU time (glibc's 32-bit one wraps at 72 minutes)
    return used.tv_sec * CLOCKS_PER_SEC + used.tv_nsec / 1000;
}