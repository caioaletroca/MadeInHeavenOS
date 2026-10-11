#ifndef _HISTORY_H_
#define _HISTORY_H_

#include <stddef.h>

#define HISTORY_MAX 32

typedef struct history
{
    char *lines[HISTORY_MAX];
    int start; // Index of the oldest entry in the history
    int count; // Number of entries currently in the history
    int total; // Total number of commands ever added to the history
} history_t;

/**
 * Adds a command to the history.
 *
 * @param history The history structure to update.
 * @param line The command line to add.
 * @return 0 on success, -1 on error.
 */
int history_add(history_t *history, const char *line);

/**
 * Shows the command history.
 *
 * @param history The history structure to display.
 * @return 0 on success, -1 on error.
 */
int history_show(history_t *history);

/**
 * Copies line into out, replacing history references: !! (the last line),
 * !n (line n, as history_show numbers it), !-n (n lines back). A '!' before
 * anything else, or inside '...', stays literal (a ' inside "..." still
 * toggles the single-quote tracking: rare, not handled).
 *
 * @param history The history structure to query.
 * @param line The command line to expand.
 * @param out The buffer for the result; always holds the line on success.
 * @param n Size of the output buffer.
 * @return 1 if something was replaced, 0 if not, -1 on an error already
 *         printed (unknown event, line too long).
 */
int history_expand(history_t *history, const char *line, char *out, size_t n);

#endif // _HISTORY_H_