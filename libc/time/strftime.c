#include <time.h>
#include <stdio.h>

static const char *const day_names[7] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
static const char *const month_names[12] = {"January", "February", "March", "April", "May", "June", "July",
                                            "August", "September", "October", "November", "December"};

typedef struct
{
    char *buffer;
    size_t size;
    size_t length;
} output_t;

/**
 * @brief Writes a character to the output buffer if there is space.
 *
 * @param out The output buffer structure.
 * @param c The character to write.
 */
static void put_char(output_t *out, char c)
{
    if (out->length + 1 < out->size)
        out->buffer[out->length] = c;

    out->length++;
}

/**
 * @brief Writes a string to the output buffer if there is space.
 *
 * @param out The output buffer structure.
 * @param text The string to write.
 * @param limit The maximum number of characters to write.
 */
static void put_text(output_t *out, const char *text, size_t limit)
{
    for (size_t i = 0; i < limit && text[i] != '\0'; i++)
        put_char(out, text[i]);
}

/**
 * @brief Writes a number to the output buffer with specified width and padding.
 *
 * @param out The output buffer structure.
 * @param value The number to write.
 * @param width The minimum width of the output (padded if necessary).
 * @param pad The character to use for padding (' ' or '0').
 */
static void put_number(output_t *out, long value, int width, char pad)
{
    char digits[32];
    snprintf(digits, sizeof(digits), pad == '0' ? "%0*ld" : "%*ld", width, value);
    put_text(out, digits, sizeof(digits));
}

/**
 * @brief Returns the name of a day or month based on the provided index.
 *
 * @param names The array of names (days or months).
 * @param count The number of names in the array.
 * @param index The index to retrieve.
 * @return The name corresponding to the index, or "?" if out of bounds.
 */
static const char *name_of(const char *const *names, int count, int index)
{
    return index >= 0 && index < count ? names[index] : "?";
}

/**
 * @brief Returns the number of days in a given year.
 *
 * @param year The year to check.
 * @return 366 if it's a leap year, otherwise 365.
 */
static int days_in_year(long year)
{
    return (year % 4 == 0 && year % 100 != 0) || year % 400 == 0 ? 366 : 365;
}

/**
 * @brief Calculates the ISO week number and year for a given date.
 *
 * @param tm The struct tm containing the date.
 * @param iso_year Pointer to store the calculated ISO year.
 * @return The ISO week number (1-53).
 */
static int iso_week(const struct tm *tm, long *iso_year)
{
    long year = tm->tm_year + 1900L;
    int from_monday = (tm->tm_wday + 6) % 7;      // 0=Monday, 6=Sunday
    int thursday = tm->tm_yday - from_monday + 3; // day of year of the Thursday in this week

    if (thursday < 0)
    {
        year--;
        thursday += days_in_year(year);
    }
    else if (thursday >= days_in_year(year))
    {
        thursday -= days_in_year(year);
        year++;
    }

    *iso_year = year;
    return thursday / 7 + 1;
}

/**
 * @brief Expands a format string into the output buffer based on the provided struct tm.
 *
 * @param out The output buffer structure.
 * @param format The format string to expand.
 * @param tm The struct tm containing date and time information.
 */
static void expand(output_t *out, const char *format, const struct tm *tm)
{
    long year = tm->tm_year + 1900L;
    long iso_year;

    for (const char *f = format; *f != '\0'; f++)
    {
        if (*f != '%')
        {
            put_char(out, *f);
            continue;
        }

        f++;
        if (*f == 'E' || *f == 'O')
            f++; // ignore locale modifiers

        switch (*f)
        {
        case '\0':
            put_char(out, '%');
            return;
        case 'a':
            put_text(out, name_of(day_names, 7, tm->tm_wday), 3);
            break;
        case 'A':
            put_text(out, name_of(day_names, 7, tm->tm_wday), 16);
            break;
        case 'b':
        case 'h':
            put_text(out, name_of(month_names, 12, tm->tm_mon), 3);
            break;
        case 'B':
            put_text(out, name_of(month_names, 12, tm->tm_mon), 16);
            break;
        case 'c':
            expand(out, "%a %b %e %H:%M:%S %Y", tm);
            break;
        case 'C':
            put_number(out, year >= 0 ? year / 100 : (year - 99) / 100, 2, '0');
            break;
        case 'd':
            put_number(out, tm->tm_mday, 2, '0');
            break;
        case 'D':
            expand(out, "%m/%d/%y", tm);
            break;
        case 'e':
            put_number(out, tm->tm_mday, 2, ' ');
            break;
        case 'F':
            expand(out, "%Y-%m-%d", tm);
            break;
        case 'g':
            iso_week(tm, &iso_year);
            put_number(out, (iso_year % 100 + 100) % 100, 2, '0');
            break;
        case 'G':
            iso_week(tm, &iso_year);
            put_number(out, iso_year, 0, '0');
            break;
        case 'H':
            put_number(out, tm->tm_hour, 2, '0');
            break;
        case 'I':
            put_number(out, tm->tm_hour % 12 == 0 ? 12 : tm->tm_hour % 12, 2, '0');
            break;
        case 'j':
            put_number(out, tm->tm_yday + 1, 3, '0');
            break;
        case 'm':
            put_number(out, tm->tm_mon + 1, 2, '0');
            break;
        case 'M':
            put_number(out, tm->tm_min, 2, '0');
            break;
        case 'n':
            put_char(out, '\n');
            break;
        case 'p':
            put_text(out, tm->tm_hour < 12 ? "AM" : "PM", 2);
            break;
        case 'r':
            expand(out, "%I:%M:%S %p", tm);
            break;
        case 'R':
            expand(out, "%H:%M", tm);
            break;
        case 's':
        {
            struct tm copy = *tm;
            put_number(out, timegm(&copy), 0, '0');
            break;
        }
        case 'S':
            put_number(out, tm->tm_sec, 2, '0');
            break;
        case 't':
            put_char(out, '\t');
            break;
        case 'T':
            expand(out, "%H:%M:%S", tm);
            break;
        case 'u':
            put_number(out, tm->tm_wday == 0 ? 7 : tm->tm_wday, 0, '0');
            break;
        case 'U':
            put_number(out, (tm->tm_yday + 7 - tm->tm_wday) / 7, 2, '0');
            break;
        case 'V':
            put_number(out, iso_week(tm, &iso_year), 2, '0');
            break;
        case 'w':
            put_number(out, tm->tm_wday, 0, '0');
            break;
        case 'W':
            put_number(out, (tm->tm_yday + 7 - (tm->tm_wday + 6) % 7) / 7, 2, '0');
            break;
        case 'x':
            expand(out, "%m/%d/%y", tm);
            break;
        case 'X':
            expand(out, "%H:%M:%S", tm);
            break;
        case 'y':
            put_number(out, (year % 100 + 100) % 100, 2, '0');
            break;
        case 'Y':
            put_number(out, year, 0, '0');
            break;
        case 'z':
            put_text(out, "+0000", 5); // UTC only
            break;
        case 'Z':
            put_text(out, "UTC", 3); // UTC only
            break;
        case '%':
            put_char(out, '%');
            break;
        default:
            put_char(out, '%');
            put_char(out, *f);
            break;
        }
    }
}

size_t strftime(char *buffer, size_t size, const char *format, const struct tm *tm)
{
    output_t out = {
        .buffer = buffer,
        .size = size,
        .length = 0};
    expand(&out, format, tm);

    if (out.length >= out.size)
        return 0;

    buffer[out.length] = '\0';
    return out.length;
}