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
 * @brief Starts a program using posix_spawn.
 *
 * @param argv The argument vector for the program.
 * @param pid Pointer to store the spawned process ID.
 * @return 0 on success, or an error code on failure.
 */
static int program_start(char *argv[], pid_t *pid)
{
    const char *name = argv[0];
    if (strchr(name, '/') != NULL)
        return posix_spawn(pid, name, NULL, NULL, argv, environ);

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
            int n = snprintf(candidate, sizeof(candidate), "%.*s/%s", (int)length, dir, name);

            // Cut off: no such program can exist under this entry
            if (n >= 0 && (size_t)n < sizeof(candidate))
            {
                int error = posix_spawn(pid, candidate, NULL, NULL, argv, environ);

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
 * @brief Prints an error message for a failed program start.
 *
 * @param name The name of the program.
 * @param error The error code.
 * @return The exit code corresponding to the error.
 */
static int program_failed(const char *name, int error)
{
    if (error == ENOENT)
    {
        fprintf(stderr, "sh: %s: command not found\n", name);
        return 127;
    }
    fprintf(stderr, "sh: %s: cannot run (error %d)\n", name, error);
    return 126;
}

/**
 * @brief Waits for a program to finish and returns its exit status.
 *
 * @param name The name of the program.
 * @param pid The process ID of the program.
 * @return The exit status of the program.
 */
static int program_wait(const char *name, pid_t pid)
{
    int status;
    if (waitpid(pid, &status, 0) != pid)
    {
        fprintf(stderr, "sh: %s: waitpid failed\n", name);
        return 1;
    }

    if (WIFSIGNALED(status))
    {
        fprintf(stderr, "sh: %s: killed by signal %d\n", name, WTERMSIG(status));
        return 128 + WTERMSIG(status);
    }
    return WEXITSTATUS(status);
}

/**
 * @brief Runs a program stage.
 *
 * @param sh The shell instance.
 * @param stage The stage to run.
 */
static void program_run(shell_t *sh, stage_t *stage)
{
    pid_t pid;
    int error = program_start(stage->argv, &pid);
    sh->status = error != 0 ? program_failed(stage->argv[0], error) : program_wait(stage->argv[0], pid);
}

/**
 * @brief Runs a pipeline of commands.
 *
 * @param sh The shell instance.
 * @param cmd The command containing the pipeline stages.
 */
static void pipeline_run(shell_t *sh, command_t *cmd)
{
    for (int i = 0; i < cmd->count; i++)
    {
        if (builtin_exists(cmd->stages[i].argv[0]))
        {
            fprintf(stderr, "sh: %s: a builtin cannot be in a pipeline yet\n", cmd->stages[i].argv[0]);
            sh->status = 1;
            return;
        }
    }

    pid_t pids[PIPELINE_MAX];
    int started = 0;
    int last = 1;
    int input = -1;

    for (int i = 0; i < cmd->count; i++)
    {
        int fds[2] = {-1, -1};
        if (i < cmd->count - 1 && pipe(fds) < 0)
        {
            fprintf(stderr, "sh: pipe: %s\n", strerror(errno));
            break;
        }

        // The child gets the pipes as stdin and stdout; the shell's own come back after the spawn
        int saved_in = dup(0);
        int saved_out = dup(1);
        if (input >= 0)
            dup2(input, 0);
        if (fds[1] >= 0)
            dup2(fds[1], 1);

        stage_t *stage = &cmd->stages[i];
        int error = program_start(stage->argv, &pids[i]);

        dup2(saved_in, 0);
        dup2(saved_out, 1);
        close(saved_in);
        close(saved_out);

        // The shell keeps no end it handed over: a reader sees EOF only once every write end is closed
        if (input >= 0)
            close(input);
        if (fds[1] >= 0)
            close(fds[1]);
        input = fds[0];

        if (error != 0)
        {
            pids[i] = -1;
            int status = program_failed(stage->argv[0], error);
            if (i == cmd->count - 1)
                last = status;
        }
        started++;
    }

    // After a failed pipe: the read end nobody was given
    if (input >= 0)
        close(input);

    for (int i = 0; i < started; i++)
    {
        if (pids[i] > 0)
        {
            int status = program_wait(cmd->stages[i].argv[0], pids[i]);
            if (i == cmd->count - 1)
                last = status;
        }
    }

    sh->status = last;
}

int command_run(shell_t *sh, command_t *cmd)
{
    if (cmd->count > 1)
        pipeline_run(sh, cmd);
    else if (!builtin_run(sh, &cmd->stages[0]))
        program_run(sh, &cmd->stages[0]);

    return 0;
}