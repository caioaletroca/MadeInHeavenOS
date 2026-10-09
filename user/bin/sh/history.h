#ifndef _HISTORY_H_
#define _HISTORY_H_

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

#endif // _HISTORY_H_