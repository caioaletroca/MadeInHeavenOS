#include "history.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
static const char *reference_event(line_history_t *history, const char *s)
{
    if (s[1] == '!')
        return line_history_get(history, history->total);
    if (s[1] == '-')
        return line_history_get(history, history->total + 1 - atoi(s + 2));

    return line_history_get(history, atoi(s + 1));
}

int history_show(line_history_t *history)
{
    if (history == NULL)
        return -1;

    for (int number = history->total - history->lines.count + 1; number <= history->total; number++)
        printf("%5d  %s\n", number, line_history_get(history, number));

    return 0;
}

int history_expand(line_history_t *history, const char *line, char *out, size_t n)
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
                fprintf(stderr, "sh: %.*s: event not found\n", (int)length, s);
                return -1;
            }

            text_length = strlen(text);
            s += length - 1;
            expanded = 1;
        }

        if (pos + text_length >= n)
        {
            fprintf(stderr, "sh: line too long\n");
            return -1;
        }

        memcpy(out + pos, text, text_length);
        pos += text_length;
    }

    out[pos] = '\0';
    return expanded;
}