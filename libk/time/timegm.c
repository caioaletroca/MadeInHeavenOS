#include <time.h>

/**
 * @brief Converts a calendar date to days since 1970-01-01 (Howard Hinnant's days_from_civil).
 *
 * @param year The year (e.g., 2026).
 * @param month The month (1-12).
 * @param day The day of the month (1-31; past the month's end just counts on).
 * @return Days since 1970-01-01, negative before it.
 */
static long days_from_civil(long year, int month, int day)
{
    year -= month <= 2; // Jan and Feb end the previous March year
    long era = (year >= 0 ? year : year - 399) / 400;
    long yoe = year - era * 400;                                              // 0-399
    long doy = (153 * (month > 2 ? month - 3 : month + 9) + 2) / 5 + day - 1; // 0-365
    long doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;                         // 0-146096
    return era * 146097 + doe - 719468;
}

time_t timegm(struct tm *tm)
{
    // Months past 11 (or below 0) carry into the year first: the rest carries through the seconds
    long year = tm->tm_year + 1900L + tm->tm_mon / 12;
    int month = tm->tm_mon % 12;
    if (month < 0)
    {
        month += 12;
        year--;
    }

    long days = days_from_civil(year, month + 1, 1) + tm->tm_mday - 1;
    time_t seconds = days * 86400 + tm->tm_hour * 3600L + tm->tm_min * 60L + tm->tm_sec;

    if (gmtime_r(&seconds, tm) == NULL)
        return -1;

    return seconds;
}

time_t mktime(struct tm *tm)
{
    return timegm(tm);
}