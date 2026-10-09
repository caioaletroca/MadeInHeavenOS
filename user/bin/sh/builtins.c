#include "sh.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Builtin command definitions for the shell
typedef int (*builtin_func_t)(shell_t *sh, int argc, char *argv[]);

/**
 * Structure representing a builtin command
 */
typedef struct builtin
{
    const char *name;
    builtin_func_t run;
    const char *help; // brief description of the builtin command
} builtin_t;

static int builtin_exit(shell_t *shell, int argc, char *argv[]);
static int builtin_help(shell_t *shell, int argc, char *argv[]);
static int builtin_history(shell_t *shell, int argc, char *argv[]);

static const builtin_t builtins[] =
    {
        {"exit", builtin_exit, "exit [n]: leave the shell with status n (default: the last status)"},
        {"help", builtin_help, "help: list the builtins"},
        {"history", builtin_history, "history: list the last commands"},
};

/**
 * Builtin command: exit
 * Usage: exit [n]
 * Leave the shell with status n (default: the last status)
 */
static int builtin_exit(shell_t *sh, int argc, char *argv[])
{
    if (argc > 2)
    {
        fprintf(stderr, "exit: too many arguments\n");
        return 1;
    }

    sh->running = 0;
    return argc > 1 ? atoi(argv[1]) : sh->status;
}

/**
 * Builtin command: help
 * Usage: help
 * List all available builtin commands with their brief descriptions
 */
static int builtin_help(shell_t *sh, int argc, char *argv[])
{
    (void)sh;   // unused parameter
    (void)argc; // unused parameter
    (void)argv; // unused parameter

    for (size_t i = 0; i < sizeof(builtins) / sizeof(builtins[0]); i++)
    {
        printf("\t%s\n", builtins[i].help);
    }

    return 0;
}

/**
 * Builtin command: history
 * Usage: history
 * List the last commands entered in the shell
 */
static int builtin_history(shell_t *sh, int argc, char *argv[])
{
    // Implementation of the history builtin command
    (void)argc; // unused parameter
    (void)argv; // unused parameter

    history_show(&sh->history);

    return 0;
}

int builtin_run(shell_t *sh, command_t *cmd)
{
    if (cmd == NULL || cmd->argc == 0)
        return -1;

    for (size_t i = 0; i < sizeof(builtins) / sizeof(builtins[0]); i++)
    {
        if (strcmp(cmd->argv[0], builtins[i].name) == 0)
        {
            sh->status = builtins[i].run(sh, cmd->argc, cmd->argv);
            return 1;
        }
    }

    return 0;
}