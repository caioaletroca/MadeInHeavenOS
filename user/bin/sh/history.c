#include "history.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int history_add(history_t *history, const char *line)
{
    if (history == NULL || line == NULL)
        return -1;

    // Only blanks: nothing worth recalling
    if (line[strspn(line, " \t")] == '\0')
        return 0;

    char *copy = strdup(line);
    if (copy == NULL)
        return -1;

    if (history->count < HISTORY_MAX)
    {
        int index = (history->start + history->count) % HISTORY_MAX;
        history->lines[index] = copy;
        history->count++;
    }
    else
    {
        free(history->lines[history->start]);
        history->lines[history->start] = copy;
        history->start = (history->start + 1) % HISTORY_MAX;
    }

    history->total++;

    return 0;
}

int history_show(history_t *history)
{
    if (history == NULL)
        return -1;

    for (int i = 0; i < history->count; i++)
    {
        int index = (history->start + i) % HISTORY_MAX;
        printf("%5d  %s\n", history->total - history->count + 1 + i, history->lines[index]);
    }

    return 0;
}