#include <line/line.h>
#include <stdlib.h>
#include <string.h>

int line_history_add(line_history_t *history, const char *line)
{
    // Only blanks: nothing worth recalling
    if (line[strspn(line, " \t")] == '\0')
        return 0;

    char *copy = strdup(line);
    if (copy == NULL)
        return -1;

    if (ring_is_full(&history->lines))
        free(ring_shift(&history->lines));

    ring_push(&history->lines, copy);
    history->total++;

    return 0;
}

const char *line_history_get(const line_history_t *history, int number)
{
    int first = history->total - history->lines.count + 1;

    if (number < first || number > history->total)
        return NULL;

    return ring_at(&history->lines, number - first);
}