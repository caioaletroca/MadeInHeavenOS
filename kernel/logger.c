#include <logger.h>
#include <stdarg.h>
#include <kprintf.h>

static int console_level = LOGGER_CONSOLE_DEFAULT;

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

    va_list args;
    va_start(args, fmt);
    kvprintf(fmt, args);
    va_end(args);
}