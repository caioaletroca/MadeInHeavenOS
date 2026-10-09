#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <spawn.h>
#include <sys/wait.h>
#include <errno.h>
#include "sh.h"

// Longest "dir/name" tried; the kernel takes 64-byte paths anyway (until the VFS)
#define PATH_CANDIDATE_MAX 256

/**
 * @brief Spawns a program using posix_spawn and waits for it to finish.
 *
 * @param sh The shell instance.
 * @param path The path to the program to run.
 * @param cmd The command to run.
 * @return 0 on success, or an error code on failure.
 */
static int program_spawn(shell_t *sh, const char *path, command_t *cmd)
{
    pid_t pid;
    int error = posix_spawn(&pid, path, NULL, NULL, cmd->argv, environ);
    if (error != 0)
        return error;

    int status;
    if (waitpid(pid, &status, 0) != pid)
    {
        fprintf(stderr, "sh: %s: waitpid failed\n", cmd->argv[0]);
        sh->status = 1;
        return 0;
    }

    if (WIFSIGNALED(status))
    {
        fprintf(stderr, "sh: %s: killed by signal %d\n", cmd->argv[0], WTERMSIG(status));
        sh->status = 128 + WTERMSIG(status);
    }
    else
    {
        sh->status = WEXITSTATUS(status);
    }

    return 0;
}

static int program_search(shell_t *sh, command_t *cmd)
{
    const char *dir = getenv("PATH");
    if (dir == NULL)
        return ENOENT;

    char candidate[PATH_CANDIDATE_MAX];

    while (1)
    {
        const char *end = strchr(dir, ':');
        size_t length = end != NULL ? (size_t)(end - dir) : strlen(dir);

        // An empty entry would mean the current directory (POSIX): none exist yet
        if (length > 0)
        {
            // %.*s prints exactly length characters of dir: PATH stays untouched
            int n = snprintf(candidate, sizeof(candidate), "%.*s/%s", (int)length, dir, cmd->argv[0]);

            // Cut off: no such program can exist under this entry
            if (n >= 0 && (size_t)n < sizeof(candidate))
            {
                int error = program_spawn(sh, candidate, cmd);
                if (error != ENOENT)
                    return error;
            }
        }

        if (end == NULL)
            break;
        dir = end + 1;
    }

    return ENOENT;
}

/**
 * @brief Runs a program command.
 *
 * @param sh The shell instance.
 * @param cmd The command to run.
 * @return 0 on success, 1 if the command contains a '/', or other error codes.
 */
static int program_run(shell_t *sh, command_t *cmd)
{
    const char *name = cmd->argv[0];

    int error = strchr(name, '/') != NULL ? program_spawn(sh, name, cmd) : program_search(sh, cmd);

    if (error == ENOENT)
    {
        fprintf(stderr, "sh: %s: command not found\n", name);
        sh->status = 127;
    }
    else if (error != 0)
    {
        fprintf(stderr, "sh: %s: cannot run (error %d)\n", name, error);
        sh->status = 126;
    }

    return 0;
}

int command_run(shell_t *sh, command_t *cmd)
{
    int builtin_result = builtin_run(sh, cmd);
    if (builtin_result == 1)
        return 0;
    else if (builtin_result == -1)
        return -1;

    return program_run(sh, cmd);
}