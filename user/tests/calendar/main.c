#include <errno.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/wait.h>
#include <test.h>

/*
 * The calendar in libc: gmtime and localtime (seconds to a date), timegm and
 * mktime (a date to seconds, normalizing out-of-range fields), strftime, and
 * the date program that prints with it. Everything is UTC.
 *
 * The dates are fixed seconds whose calendar is known: the epoch, the leap
 * days of 2000 (a leap year, divisible by 400) and 2100 (not one, divisible
 * by 100), 2038's signed 32-bit limit, and dates before 1970.
 */

#define DAY 86400L
#define NOV_10_2026 1794268800L // a Tuesday, day 313 of 2026
#define JAN_1_2027 1798761600L  // a Friday: still the ISO week 53 of 2026

// gmtime_r of t matches the date, the time and the weekday and year day given
static int date_is(time_t t, int year, int month, int day, int hour, int minute, int second, int wday, int yday)
{
    struct tm tm;
    memset(&tm, 0x55, sizeof(tm)); // every field must be written
    if (gmtime_r(&t, &tm) != &tm)
        return 0;

    return tm.tm_year == year - 1900 && tm.tm_mon == month - 1 && tm.tm_mday == day && tm.tm_hour == hour &&
           tm.tm_min == minute && tm.tm_sec == second && tm.tm_wday == wday && tm.tm_yday == yday && tm.tm_isdst == 0;
}

static void test_gmtime(void)
{
    // Weekdays: 0 Sunday … 6 Saturday
    CHECK(date_is(0, 1970, 1, 1, 0, 0, 0, 4, 0));
    CHECK(date_is(-1, 1969, 12, 31, 23, 59, 59, 3, 364)); // rounded down to the day before, not toward 0
    CHECK(date_is(-365 * DAY, 1969, 1, 1, 0, 0, 0, 3, 0));
    CHECK(date_is(1000000000, 2001, 9, 9, 1, 46, 40, 0, 251));
    CHECK(date_is(NOV_10_2026, 2026, 11, 10, 0, 0, 0, 2, 313));
    CHECK(date_is(NOV_10_2026 - 1, 2026, 11, 9, 23, 59, 59, 1, 312));

    // Leap days: 2000 has one (divisible by 400), 2100 does not (divisible by 100 only)
    CHECK(date_is(951782400, 2000, 2, 29, 0, 0, 0, 2, 59));
    CHECK(date_is(951782400 + DAY, 2000, 3, 1, 0, 0, 0, 3, 60));
    CHECK(date_is(4107542400 - DAY, 2100, 2, 28, 0, 0, 0, 0, 58));
    CHECK(date_is(4107542400, 2100, 3, 1, 0, 0, 0, 1, 59));
    CHECK(date_is(4107542400 + 306 * DAY - 1, 2100, 12, 31, 23, 59, 59, 5, 364)); // 365 days in 2100

    // Past a signed 32-bit time_t (2038), and long before 1970: 0001-01-01, a Monday
    CHECK(date_is(2147483647, 2038, 1, 19, 3, 14, 7, 2, 18));
    CHECK(date_is(2147483648, 2038, 1, 19, 3, 14, 8, 2, 18));
    CHECK(date_is(-62135596800, 1, 1, 1, 0, 0, 0, 1, 0));

    // A year past an int: NULL, EOVERFLOW
    struct tm tm;
    time_t huge = 0x7fffffffffffffff;
    errno = 0;
    CHECK(gmtime_r(&huge, &tm) == NULL && errno == EOVERFLOW);
    huge = -huge;
    errno = 0;
    CHECK(gmtime(&huge) == NULL && errno == EOVERFLOW);

    // gmtime shares one struct; localtime is UTC
    time_t t = NOV_10_2026;
    struct tm *shared = gmtime(&t);
    CHECK(shared != NULL && shared->tm_mday == 10);
    t = 0;
    CHECK(gmtime(&t) == shared && shared->tm_year == 70);

    struct tm local, utc;
    t = 1000000000;
    CHECK(localtime_r(&t, &local) == &local && gmtime_r(&t, &utc) == &utc);
    CHECK(memcmp(&local, &utc, sizeof(local)) == 0);
    CHECK(localtime(&t) != NULL && localtime(&t)->tm_hour == 1);
}

static void test_timegm(void)
{
    // Every date back to its seconds
    static const time_t samples[] = {0, -1, 1000000000, NOV_10_2026, 951782400, 4107542400, 2147483648, -62135596800};
    for (size_t i = 0; i < sizeof(samples) / sizeof(samples[0]); i++)
    {
        struct tm tm;
        CHECK(gmtime_r(&samples[i], &tm) != NULL);
        CHECK(timegm(&tm) == samples[i]);
        CHECK(mktime(&tm) == samples[i]);
    }

    // Out of range fields carry over, and the struct comes back normalized with its weekday
    struct tm tm = {.tm_year = 126, .tm_mon = 12, .tm_mday = 1, .tm_wday = 99, .tm_yday = 99}; // month 12 of 2026
    CHECK(timegm(&tm) == JAN_1_2027);
    CHECK(tm.tm_year == 127 && tm.tm_mon == 0 && tm.tm_mday == 1 && tm.tm_wday == 5 && tm.tm_yday == 0);

    tm = (struct tm){.tm_year = 124, .tm_mon = 2, .tm_mday = 0}; // March 0, 2024: February's last day
    CHECK(timegm(&tm) != -1 && tm.tm_mon == 1 && tm.tm_mday == 29 && tm.tm_yday == 59);

    tm = (struct tm){.tm_year = 126, .tm_mon = -1, .tm_mday = 25}; // month -1: December of the year before
    CHECK(timegm(&tm) != -1 && tm.tm_year == 125 && tm.tm_mon == 11 && tm.tm_mday == 25);

    tm = (struct tm){.tm_year = 126, .tm_mon = 10, .tm_mday = 9, .tm_hour = 24}; // 24:00 is the next midnight
    CHECK(timegm(&tm) == NOV_10_2026 && tm.tm_hour == 0 && tm.tm_mday == 10);

    tm = (struct tm){.tm_year = 126, .tm_mon = 10, .tm_mday = 10, .tm_sec = -1}; // second -1: the day before
    CHECK(timegm(&tm) == NOV_10_2026 - 1 && tm.tm_mday == 9 && tm.tm_sec == 59);

    tm = (struct tm){.tm_year = 70, .tm_mday = 1, .tm_isdst = 1}; // mktime: UTC has no daylight saving
    CHECK(mktime(&tm) == 0 && tm.tm_isdst == 0);

    // A year past an int: -1, EOVERFLOW
    tm = (struct tm){.tm_year = __INT_MAX__, .tm_mon = 12, .tm_mday = 1};
    errno = 0;
    CHECK(timegm(&tm) == -1 && errno == EOVERFLOW);
}

// strftime of t with format equals expected, its length returned
static int formats_as(time_t t, const char *format, const char *expected)
{
    struct tm tm;
    char text[128];
    if (gmtime_r(&t, &tm) == NULL)
        return 0;

    size_t length = strftime(text, sizeof(text), format, &tm);
    if (length != strlen(expected) || strcmp(text, expected) != 0)
    {
        printf("strftime \"%s\": \"%s\", expected \"%s\"\n", format, text, expected);
        return 0;
    }
    return 1;
}

static void test_strftime(void)
{
    time_t t = NOV_10_2026 + 13 * 3600 + 5 * 60 + 9; // Tuesday 2026-11-10 13:05:09

    CHECK(formats_as(t, "%a %A %b %h %B", "Tue Tuesday Nov Nov November"));
    CHECK(formats_as(t, "%c", "Tue Nov 10 13:05:09 2026"));
    CHECK(formats_as(t, "%C %y %Y", "20 26 2026"));
    CHECK(formats_as(t, "%d|%e|%j|%m", "10|10|314|11"));
    CHECK(formats_as(t, "%D %F", "11/10/26 2026-11-10"));
    CHECK(formats_as(t, "%H %I %M %S %p", "13 01 05 09 PM"));
    CHECK(formats_as(t, "%r|%R|%T", "01:05:09 PM|13:05|13:05:09"));
    CHECK(formats_as(t, "%x %X", "11/10/26 13:05:09"));
    CHECK(formats_as(t, "%s", "1794315909"));
    CHECK(formats_as(t, "%u %w", "2 2"));
    CHECK(formats_as(t, "%U %W %V %G %g", "45 45 46 2026 26"));
    CHECK(formats_as(t, "%z %Z", "+0000 UTC"));
    CHECK(formats_as(t, "a%nb%tc%%d", "a\nb\tc%d"));
    CHECK(formats_as(t, "", ""));
    CHECK(formats_as(t, "no conversions", "no conversions"));

    // The C locale's modifiers change nothing; an unknown conversion and a trailing % are kept as written
    CHECK(formats_as(t, "%Ey %EY %Od %OH", "26 2026 10 13"));
    CHECK(formats_as(t, "%Q|%", "%Q|%"));

    // Padding: %e with a space, %j to 3; midnight and noon on a 12-hour clock
    CHECK(formats_as(0, "%d|%e|%j", "01| 1|001"));
    CHECK(formats_as(0, "%I %p", "12 AM"));
    CHECK(formats_as(12 * 3600, "%I %p", "12 PM"));
    CHECK(formats_as(0, "%U %W %V %G", "00 00 01 1970")); // a Thursday: before 1970's first Sunday and Monday
    CHECK(formats_as(-62135596800, "%Y %C %y %F", "1 00 01 1-01-01"));

    // ISO weeks across New Year: a week belongs to the year of its Thursday
    CHECK(formats_as(JAN_1_2027, "%a %V %G %g", "Fri 53 2026 26"));
    CHECK(formats_as(JAN_1_2027 + 2 * DAY, "%a %V %G", "Sun 53 2026"));
    CHECK(formats_as(JAN_1_2027 + 3 * DAY, "%a %V %G", "Mon 01 2027"));
    CHECK(formats_as(1735516800 - DAY, "%F %a %V %G", "2024-12-29 Sun 52 2024"));
    CHECK(formats_as(1735516800, "%F %a %V %G", "2024-12-30 Mon 01 2025"));

    // Fields out of range: "?" for a name
    struct tm odd = {.tm_mon = 12, .tm_wday = 9, .tm_mday = 1};
    char text[32];
    CHECK(strftime(text, sizeof(text), "%a %b", &odd) == 3 && strcmp(text, "? ?") == 0);

    // The buffer: the text and its '\0' must fit, or 0, and nothing is written past size
    struct tm tm;
    gmtime_r(&t, &tm);
    memset(text, '#', sizeof(text));
    CHECK(strftime(text, 11, "%F", &tm) == 10 && strcmp(text, "2026-11-10") == 0);
    memset(text, '#', sizeof(text));
    CHECK(strftime(text, 10, "%F", &tm) == 0 && text[9] == '#' && text[10] == '#');
    memset(text, '#', sizeof(text));
    CHECK(strftime(text, 0, "%F", &tm) == 0 && text[0] == '#');
    CHECK(strftime(text, sizeof(text), "", &tm) == 0 && text[0] == '\0'); // empty, not too long
}

/**
 * @brief Runs /bin/date with argv, its standard output read into out
 *
 * @return Its exit status, or -1 if it could not run or did not exit
 */
static int run_date(char *const argv[], char *out, size_t size)
{
    int fds[2];
    if (pipe(fds) != 0)
        return -1;

    pid_t child = fork();
    if (child == 0)
    {
        dup2(fds[1], 1);
        close(fds[0]);
        close(fds[1]);
        execv("/bin/date", argv);
        _exit(127);
    }
    close(fds[1]);

    size_t length = 0;
    ssize_t got;
    while (length + 1 < size && (got = read(fds[0], out + length, size - 1 - length)) > 0)
        length += (size_t)got;
    out[length] = '\0';
    close(fds[0]);

    int status;
    if (child < 0 || waitpid(child, &status, 0) != child || !WIFEXITED(status))
        return -1;
    return WEXITSTATUS(status);
}

static void test_date(void)
{
    char out[512];

    // +%s: the seconds now, between the clock before and after
    time_t before = time(NULL);
    CHECK(run_date((char *[]){"date", "+%s", NULL}, out, sizeof(out)) == 0);
    time_t after = time(NULL);
    long printed = 0; // no strtol in libc yet
    for (const char *digit = out; *digit >= '0' && *digit <= '9'; digit++)
        printed = printed * 10 + (*digit - '0');
    CHECK(printed >= before && printed <= after && out[strlen(out) - 1] == '\n');

    // The default: "Tue Nov 10 13:05:09 UTC 2026"
    CHECK(run_date((char *[]){"date", NULL}, out, sizeof(out)) == 0);
    CHECK(strlen(out) == 29 && out[28] == '\n' && strncmp(out + 19, " UTC ", 5) == 0);

    // -u is accepted (UTC already); an empty format prints an empty line
    char year[8];
    time_t now = time(NULL);
    strftime(year, sizeof(year), "%Y\n", gmtime(&now));
    CHECK(run_date((char *[]){"date", "-u", "+%Y", NULL}, out, sizeof(out)) == 0 && strcmp(out, year) == 0);
    CHECK(run_date((char *[]){"date", "+", NULL}, out, sizeof(out)) == 0 && strcmp(out, "\n") == 0);

    // Errors: status 1, nothing on standard output
    CHECK(run_date((char *[]){"date", "-x", NULL}, out, sizeof(out)) == 1 && out[0] == '\0');
    static char long_format[302];
    long_format[0] = '+';
    memset(long_format + 1, 'x', 300);
    CHECK(run_date((char *[]){"date", long_format, NULL}, out, sizeof(out)) == 1 && out[0] == '\0');
}

int main(void)
{
    test_gmtime();
    test_timegm();
    test_strftime();
    test_date();
    return test_status();
}
