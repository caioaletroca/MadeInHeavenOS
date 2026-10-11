#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "sh.h"

// Variable names (POSIX): a letter or '_', then letters, digits and '_'
#define NAME_START "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ_"
#define NAME_CHARS NAME_START "0123456789"

/**
 * Enumeration representing the state of quoting in the parser.
 */
typedef enum
{
    STATE_UNQUOTED,
    STATE_SINGLE, // inside '...': everything literal
    STATE_DOUBLE, // inside "...": \ escapes " \ $ (and later $ expands)
} quote_state_t;

/**
 * Structure representing the state of the command parser.
 */
typedef struct parser
{
    command_t *cmd;
    size_t pos;        // next free byte in cmd->words
    size_t start;      // where the open word begins in cmd->words
    int in_word;       // a word is open, even an empty one ('' or "")
    const char *error; // set when a helper fails
} parser_t;

/**
 * Opens a new word in the parser if not already in a word.
 *
 * @param p The parser state.
 */
static void word_open(parser_t *p)
{
    if (!p->in_word)
    {
        p->start = p->pos;
        p->in_word = 1;
    }
}

/**
 * Appends a character to the current word in the parser.
 *
 * @param p The parser state.
 * @param c The character to append.
 * @return 0 on success, -1 on error.
 */
static int word_append(parser_t *p, char c)
{
    if (p->pos + 1 >= sizeof(p->cmd->words))
    {
        p->error = "line too long";
        return -1;
    }

    word_open(p);
    p->cmd->words[p->pos++] = c;
    return 0;
}

/**
 * Appends a null-terminated string to the current word in the parser.
 *
 * @param p The parser state.
 * @param str The string to append.
 * @return 0 on success, -1 on error.
 */
static int word_append_string(parser_t *p, const char *str)
{
    while (*str != '\0')
    {
        if (word_append(p, *str) < 0)
            return -1;
        str++;
    }
    return 0;
}

/**
 * Closes the current word in the parser.
 *
 * @param p The parser state.
 * @return 0 on success, -1 on error.
 */
static int word_close(parser_t *p)
{
    if (!p->in_word)
        return 0;

    if (p->cmd->argc == MAX_ARGS)
    {
        p->error = "too many arguments";
        return -1;
    }

    if (p->pos >= sizeof(p->cmd->words))
    {
        p->error = "line too long";
        return -1;
    }

    p->cmd->words[p->pos++] = '\0';
    p->cmd->argv[p->cmd->argc++] = &p->cmd->words[p->start];
    p->in_word = 0;
    return 0;
}

/**
 * Expands a variable in the parser: $? (last status), $$ (the shell's pid),
 * $NAME (environment, nothing if unset). Any other '$' stays literal.
 * No field splitting: a value with spaces stays one word.
 *
 * @param p The parser state.
 * @param sh The shell state.
 * @param s Points at the '$' on entry, at the last character used on return
 *          (the caller's loop steps past it).
 * @return 0 on success, -1 on error (p->error set).
 */
static int expand(parser_t *p, shell_t *sh, const char **s)
{
    const char next = *(*s + 1);
    if (next == '?')
    {
        char number[12];
        snprintf(number, sizeof(number), "%d", sh->status);
        if (word_append_string(p, number) < 0)
            return -1;
        *s += 1; // skip the '?'
        return 0;
    }
    else if (next == '$')
    {
        char number[12];
        snprintf(number, sizeof(number), "%d", getpid());
        if (word_append_string(p, number) < 0)
            return -1;
        *s += 1; // skip the '$'
        return 0;
    }
    else if (next == '_' || (next >= 'a' && next <= 'z') || (next >= 'A' && next <= 'Z') || (next >= '0' && next <= '9'))
    {
        const char *name = *s + 1; // skip the '$' character

        if (*name != '\0' && strchr(NAME_START, *name) != NULL)
        {
            size_t length = strspn(name, NAME_CHARS);

            char buffer[256];

            snprintf(buffer, sizeof(buffer), "%.*s", (int)length, name);

            const char *value = getenv(buffer);
            if (value != NULL && word_append_string(p, value) < 0)
                return -1;

            *s = name + length - 1;

            return 0;
        }
    }

    if (word_append(p, '$') < 0)
        return -1;

    return 0;
}

read_result_t command_read(command_t *cmd)
{
    if (fgets(cmd->command, COMMAND_MAX, stdin) == NULL)
    {
        printf("\n");
        return READ_EOF;
    }

    // Ends the string at the first '\n' (or leaves it if the line had none)
    cmd->command[strcspn(cmd->command, "\n")] = '\0';

    return READ_COMMAND;
}

read_result_t command_parse(shell_t *sh, command_t *cmd)
{
    parser_t parser = {
        .cmd = cmd};
    quote_state_t state = STATE_UNQUOTED;
    cmd->argc = 0;

    for (const char *s = cmd->command; *s != '\0'; s++)
    {
        char c = *s;
        int ret = 0;

        switch (state)
        {
        case STATE_UNQUOTED:
            if (c == ' ' || c == '\t')
                ret = word_close(&parser);
            else if (c == '\'')
            {
                word_open(&parser);
                state = STATE_SINGLE;
            }
            else if (c == '"')
            {
                word_open(&parser);
                state = STATE_DOUBLE;
            }
            else if (c == '$')
                ret = expand(&parser, sh, &s);
            else if (c == '\\' && s[1] != '\0')
                ret = word_append(&parser, *++s);
            else
                ret = word_append(&parser, c);
            break;
        case STATE_SINGLE:
            if (c == '\'')
                state = STATE_UNQUOTED;
            else
                ret = word_append(&parser, c);
            break;
        case STATE_DOUBLE:
            if (c == '"')
                state = STATE_UNQUOTED;
            else if (c == '\\' && (s[1] == '"' || s[1] == '\\' || s[1] == '$'))
                ret = word_append(&parser, *++s);
            else if (c == '$')
                ret = expand(&parser, sh, &s);
            else
                ret = word_append(&parser, c);
            break;
        default:
            break;
        }

        if (ret < 0)
        {
            printf("sh: %s\n", parser.error);
            return READ_NOTHING;
        }
    }

    if (state != STATE_UNQUOTED)
    {
        printf("sh: unmatched quote\n");
        return READ_NOTHING;
    }

    if (word_close(&parser) < 0)
    {
        printf("sh: %s\n", parser.error);
        return READ_NOTHING;
    }

    cmd->argv[cmd->argc] = NULL;
    return cmd->argc == 0 ? READ_NOTHING : READ_COMMAND;
}