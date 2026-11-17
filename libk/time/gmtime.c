#include <time.h>
#include <errno.h>

/**
 * @brief Converts days since 1970-01-01 to a calendar date (Howard Hinnant's civil_from_days).
 *
 * Counts years from March 1: February, and its leap day, is then the year's last month.
 *
 * @param days Days since 1970-01-01, negative before it.
 * @param year Receives the year (e.g., 2026).
 * @param month Receives the month (1-12).
 * @param day Receives the day of the month (1-31).
 */
static void civil_from_days(long days, long *year, int *month, int *day)
{
    days += 719468;                                                   // days since 0000-03-01
    long era = (days >= 0 ? days : days - 146096) / 146097;           // 400-year era, rounded down
    long doe = days - era * 146097;                                   // [0, 146096] days of era
    long yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365; // [0, 399] year of era
    long doy = doe - (365 * yoe + yoe / 4 - yoe / 100);               // [0, 365] day of year
    long mp = (5 * doy + 2) / 153;                                    // [0, 11] month of year

    *day = (int)(doy - (153 * mp + 2) / 5 + 1); // [1, 31] day of month
    *month = (int)(mp < 10 ? mp + 3 : mp - 9);  // [1, 12] month of year
    *year = yoe + era * 400 + (*month <= 2);    // year since 0000
}

/**
 * @brief Returns whether a year is a leap year.
 *
 * @param year The year to check.
 * @return 1 if the year is a leap year, 0 otherwise.
 */
static int is_leap(long year)
{
    return (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
}

struct tm *gmtime_r(const time_t *timer, struct tm *result)
{
    // Days before each month, in a common year
    static const short days_before[12] = {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334};

    long days = *timer / 86400;
    long seconds = *timer % 86400;
    // rounded down: -1 is 23:59:59 of the day before, not of day 0
    if (seconds < 0)
    {
        seconds += 86400;
        days--;
    }

    long year;
    int month, day;
    civil_from_days(days, &year, &month, &day);

    // Check for overflow when converting year to int (tm_year is years since 1900)
    if (year - 1900 > __INT_MAX__ || year - 1900 < -__INT_MAX__ - 1)
    {
        errno = EOVERFLOW;
        return NULL;
    }

    result->tm_sec = (int)(seconds % 60);
    result->tm_min = (int)((seconds / 60) % 60);
    result->tm_hour = (int)(seconds / 3600);
    result->tm_mday = day;
    result->tm_mon = month - 1;
    result->tm_year = (int)(year - 1900);
    result->tm_wday = (int)((days % 7 + 11) % 7); // 1970-01-01 was a Thursday
    result->tm_yday = days_before[month - 1] + day - 1 + (month > 2 && is_leap(year));
    result->tm_isdst = 0; // UTC has no DST
    return result;
}

struct tm *gmtime(const time_t *timer)
{
    static struct tm shared;
    return gmtime_r(timer, &shared);
}