#include <logger.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdbool.h>
#include <driver/clock.h>
#include <kprintf.h>

#define LOGGER_BUFFER_SIZE 512

static int console_level = LOGGER_CONSOLE_DEFAULT;

// The last message shown did not end its line: the next one continues it, without a timestamp
static bool line_open;

void logger_set_console_level(int cutoff)
{
    if (cutoff < 0 || cutoff > LOGGER_CONSOLE_MAX)
        return;

    console_level = cutoff;
}

void log_message(logger_level_t level, const char *fmt, ...)
{
    if ((int)level >= console_level)
        return;

    char out[LOGGER_BUFFER_SIZE];
    int n = 0;

    if (!line_open)
    {
        int64_t now = clock_monotonic();
        n = snprintf(out, sizeof(out),
                     "[%5lld.%06lld] ",
                     (long long)(now / NANOSECONDS_PER_SECOND),
                     (long long)(now % NANOSECONDS_PER_SECOND / 1000));
    }

    va_list args;
    va_start(args, fmt);
    n += vsnprintf(out + n, sizeof(out) - n, fmt, args);
    va_end(args);

    // n is the full length: a longer message was cut to the buffer
    if (n >= (int)sizeof(out))
        n = sizeof(out) - 1;

    if (n > 0)
    {
        line_open = out[n - 1] != '\n';
        tty_write(out, n);
    }
}