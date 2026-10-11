#include "history.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * Retrieves a command from the history by its index.
 *
 * @param history The history structure to query.
 * @param index The index of the command to retrieve (0 is the oldest).
 * @return The command line at the given index, or NULL if out of bounds.
 */
static const char *history_get(history_t *history, int index)
{
    int first = history->total - history->count + 1; // number of the oldest stored line
    if (index < first || index > history->total)
        return NULL;

    return history->lines[(history->start + index - first) % HISTORY_MAX];
}

/**
 * Determines the length of a history reference at the start of a string.
 *
 * @param s The string to examine.
 * @return The length of the history reference, or 0 if none is found.
 */
static size_t reference_length(const char *s)
{
    if (s[0] != '!')
        return 0;
    if (s[1] == '!')
        return 2;

    const char *digits = s[1] == '-' ? s + 2 : s + 1;
    size_t length = strspn(digits, "0123456789");
    return length > 0 ? (size_t)(digits - s) + length : 0;
}

/**
 * Retrieves the command corresponding to a history reference.
 *
 * @param history The history structure to query.
 * @param s The history reference string (starting with '!').
 * @return The command line corresponding to the reference, or NULL if not found.
 */
static const char *reference_event(history_t *history, const char *s)
{
    if (s[1] == '!')
        return history_get(history, history->total);
    if (s[1] == '-')
        return history_get(history, history->total + 1 - atoi(s + 2));

    return history_get(history, atoi(s + 1));
}

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

int history_expand(history_t *history, const char *line, char *out, size_t n)
{
    size_t pos = 0;
    int expanded = 0;
    int quoted = 0;

    for (const char *s = line; *s != '\0'; s++)
    {
        if (*s == '\'')
            quoted = !quoted;

        const char *text = s;
        size_t text_length = 1;

        size_t length = quoted ? 0 : reference_length(s);
        if (length > 0)
        {
            text = reference_event(history, s);
            if (text == NULL)
            {
                printf("sh: %.*s: event not found\n", (int)length, s);
                return -1;
            }

            text_length = strlen(text);
            s += length - 1;
            expanded = 1;
        }

        if (pos + text_length >= n)
        {
            printf("sh: line too long\n");
            return -1;
        }

        memcpy(out + pos, text, text_length);
        pos += text_length;
    }

    out[pos] = '\0';
    return expanded;
}