#include "sh.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

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
static int builtin_cd(shell_t *shell, int argc, char *argv[]);
static int builtin_pwd(shell_t *shell, int argc, char *argv[]);
static int builtin_fg(shell_t *shell, int argc, char *argv[]);
static int builtin_jobs(shell_t *shell, int argc, char *argv[]);

static const builtin_t builtins[] =
    {
        {"cd", builtin_cd, "cd [dir]: change the current directory (default: $HOME, else /)"},
        {"exit", builtin_exit, "exit [n]: leave the shell with status n (default: the last status)"},
        {"fg", builtin_fg, "fg: resume the last stopped job"},
        {"help", builtin_help, "help: list the builtins"},
        {"history", builtin_history, "history: list the last commands"},
        {"jobs", builtin_jobs, "jobs: list the stopped jobs"},
        {"pwd", builtin_pwd, "pwd: print the current directory"},
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

/**
 * Builtin command: cd
 * Usage: cd [dir]
 * Change the current directory to 'dir' (default: $HOME, else /)
 */
static int builtin_cd(shell_t *sh, int argc, char *argv[])
{
    (void)sh;

    if (argc > 2)
    {
        fprintf(stderr, "cd: too many arguments\n");
        return 1;
    }

    const char *dir = argc > 1 ? argv[1] : getenv("HOME");
    if (dir == NULL || dir[0] == '\0')
        dir = "/";

    if (chdir(dir) < 0)
    {
        fprintf(stderr, "cd: %s: %s\n", dir, strerror(errno));
        return 1;
    }

    return 0;
}

/**
 * Builtin command: pwd
 * Usage: pwd
 * Print the current working directory
 */
static int builtin_pwd(shell_t *sh, int argc, char *argv[])
{
    (void)sh;   // unused parameter
    (void)argc; // unused parameter
    (void)argv; // unused parameter

    char path[CWD_MAX];
    if (getcwd(path, sizeof(path)) == NULL)
    {
        fprintf(stderr, "pwd: %s\n", strerror(errno));
        return 1;
    }

    printf("%s\n", path);
    return 0;
}

/**
 * Builtin command: fg
 * Usage: fg
 * Resume the last stopped job
 */
static int builtin_fg(shell_t *sh, int argc, char *argv[])
{
    (void)argc; // unused parameter
    (void)argv; // unused parameter

    if (sh->job_count == 0)
    {
        fprintf(stderr, "fg: no stopped job\n");
        return 1;
    }

    job_t job = sh->jobs[--sh->job_count];
    return job_resume(sh, &job);
}

/**
 * Builtin command: jobs
 * Usage: jobs
 * List the stopped jobs
 */
static int builtin_jobs(shell_t *sh, int argc, char *argv[])
{
    (void)argc; // unused parameter
    (void)argv; // unused parameter

    for (int i = 0; i < sh->job_count; i++)
        printf("[%d]  Stopped  %s\n", i + 1, sh->jobs[i].command);

    return 0;
}

/**
 * Lookup a builtin command by name.
 *
 * @param name The name of the builtin command.
 * @return A pointer to the builtin_t structure if found, NULL otherwise.
 */
static const builtin_t *builtin_lookup(const char *name)
{
    for (size_t i = 0; i < sizeof(builtins) / sizeof(builtins[0]); i++)
        if (strcmp(name, builtins[i].name) == 0)
            return &builtins[i];

    return NULL;
}

int builtin_run(shell_t *sh, stage_t *stage)
{
    const builtin_t *builtin = builtin_lookup(stage->argv[0]);
    if (builtin == NULL)
        return 0;

    sh->status = builtin->run(sh, stage->argc, stage->argv);
    return 1;
}