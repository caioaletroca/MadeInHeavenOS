#include <time.h>

// MiHOS knows no time zone (no TZ, no /etc/localtime): local time is UTC

struct tm *localtime_r(const time_t *timer, struct tm *result)
{
    return gmtime_r(timer, result);
}

struct tm *localtime(const time_t *timer)
{
    static struct tm shared;
    return gmtime_r(timer, &shared);
}