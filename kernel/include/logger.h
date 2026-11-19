#ifndef _LOGGER_H_
#define _LOGGER_H_

/**
 * A kernel message's level, Linux's eight with plain names: the lower, the
 * more serious. loglevel=N on the command line uses the same numbers.
 */
typedef enum logger_level
{
    LOG_EMERGENCY,
    LOG_ALERT,
    LOG_CRITICAL,
    LOG_ERROR,
    LOG_WARNING,
    LOG_NOTICE,
    LOG_INFO,
    LOG_DEBUG,
} logger_level_t;

/*
 * The console's cutoff: a message is shown when its level is below it.
 * The default shows everything but debug; quiet shows errors and worse;
 * loglevel=8 shows debug too.
 */
#define LOGGER_CONSOLE_DEFAULT 7
#define LOGGER_CONSOLE_QUIET 4
#define LOGGER_CONSOLE_MAX 8

/**
 * @brief Sets the console's cutoff (quiet, loglevel=).
 *
 * @param cutoff 0 to LOGGER_CONSOLE_MAX; others are ignored.
 */
void logger_set_console_level(int cutoff);

/**
 * @brief Prints a kernel message on the console when its level is below the
 * console's cutoff.
 *
 * @param level How serious the message is.
 * @param fmt printf's format, then its arguments.
 */
void log_message(logger_level_t level, const char *fmt, ...);

#define log_error(...) log_message(LOG_ERROR, __VA_ARGS__)
#define log_warning(...) log_message(LOG_WARNING, __VA_ARGS__)
#define log_notice(...) log_message(LOG_NOTICE, __VA_ARGS__)
#define log_info(...) log_message(LOG_INFO, __VA_ARGS__)
#define log_debug(...) log_message(LOG_DEBUG, __VA_ARGS__)

#endif // _LOGGER_H_