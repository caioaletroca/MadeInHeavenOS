#include <stdio.h>
#include <string.h>
#include "sh.h"

/**
 * Splits a line into arguments.
 *
 * @param line The input line to split.
 * @param argv The array to store the arguments.
 * @param max The maximum number of arguments.
 * @return The number of arguments, or -1 if too many arguments.
 */
static int line_split(char *line, char *argv[], int max)
{
    int argc = 0;
    char *save;

    for (char *token = strtok_r(line, " \t\n", &save); token != NULL; token = strtok_r(NULL, " \t\n", &save))
    {
        if (argc == max)
            return -1;

        argv[argc++] = token;
    }

    argv[argc] = NULL;
    return argc;
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

read_result_t command_parse(command_t *cmd)
{
    int argc = line_split(cmd->command, cmd->argv, MAX_ARGS);

    if (argc == 0)
        return READ_NOTHING;

    if (argc < 0)
    {
        printf("sh: too many arguments\n");
        return READ_NOTHING;
    }

    cmd->argc = argc;
    return READ_COMMAND;
}