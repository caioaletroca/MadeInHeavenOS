#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <errno.h>
#include "sh.h"

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
        // A reader that left is normal in a pipeline, not worth a line
        if (WTERMSIG(status) != SIGPIPE)
            fprintf(stderr, "sh: %s: killed by signal %d\n", name, WTERMSIG(status));
        return 128 + WTERMSIG(status);
    }
    return WEXITSTATUS(status);
}

static void stage_run(shell_t *sh, stage_t *stage)
{
    if (builtin_run(sh, stage))
    {
        fflush(stdout);
        _exit(sh->status);
    }

    execvp(stage->argv[0], stage->argv);

    // Back here: it failed, and only this child knows
    _exit(program_failed(stage->argv[0], errno));
}

/**
 * @brief Runs a pipeline of commands.
 *
 * @param sh The shell instance.
 * @param cmd The command containing the pipeline stages.
 */
static void pipeline_run(shell_t *sh, command_t *cmd)
{
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

        pid_t pid = fork();
        if (pid < 0)
        {
            fprintf(stderr, "sh: fork: %s\n", strerror(errno));
            if (fds[0] >= 0)
            {
                close(fds[0]);
                close(fds[1]);
            }
            break;
        }

        if (pid == 0)
        {
            if (input >= 0)
            {
                dup2(input, 0);
                close(input);
            }
            if (fds[1] >= 0)
            {
                dup2(fds[1], 1);
                close(fds[1]);
            }

            // Its own output's read end: kept, it would be a reader that never reads
            if (fds[0] >= 0)
                close(fds[0]);

            stage_run(sh, &cmd->stages[i]);
        }

        pids[started++] = pid;

        // The shell keeps no end it handed over: a reader sees EOF only once every write end is closed
        if (input >= 0)
            close(input);
        if (fds[1] >= 0)
            close(fds[1]);
        input = fds[0];
    }

    // After a failed pipe: the read end nobody was given
    if (input >= 0)
        close(input);

    for (int i = 0; i < started; i++)
    {
        int status = program_wait(cmd->stages[i].argv[0], pids[i]);
        if (i == cmd->count - 1)
            last = status;
    }

    sh->status = last;
}

int command_run(shell_t *sh, command_t *cmd)
{
    // Alone, a builtin runs in the shell itself: cd and exit must change this process
    if (cmd->count > 1 || !builtin_run(sh, &cmd->stages[0]))
        pipeline_run(sh, cmd);

    return 0;
}