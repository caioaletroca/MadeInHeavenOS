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
        // A reader that left is normal in a pipeline, and whoever pressed Ctrl+C knows
        if (WTERMSIG(status) != SIGPIPE && WTERMSIG(status) != SIGINT)
            fprintf(stderr, "sh: %s: killed by signal %d\n", name, WTERMSIG(status));
        return 128 + WTERMSIG(status);
    }

    return WEXITSTATUS(status);
}

/**
 * @brief Sets up a stage's process, in the child, before it runs: its pipeline's
 * group, and the signals back to normal. SIG_IGN survives exec, and a builtin
 * in a pipeline has no exec to reset the handler.
 *
 * @param group The pipeline's group, 0 for the first stage: it starts the group.
 */
static void stage_job_setup(pid_t group)
{
    setpgid(0, group);
    signal(SIGINT, SIG_DFL);
    signal(SIGQUIT, SIG_DFL);
    signal(SIGTTOU, SIG_DFL);
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
    pid_t group = 0;

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
            if (sh->interactive)
                stage_job_setup(group);

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

        // Here too, whichever side runs first. A child already past exec answers EACCES, and it is done
        if (sh->interactive)
        {
            if (group == 0)
                group = pid;

            setpgid(pid, group);
            tcsetpgrp(0, group);
        }

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

    if (sh->interactive)
        tcsetpgrp(0, sh->group);

    sh->status = last;
}

int command_run(shell_t *sh, command_t *cmd)
{
    // Alone, a builtin runs in the shell itself: cd and exit must change this process
    if (cmd->count > 1 || !builtin_run(sh, &cmd->stages[0]))
        pipeline_run(sh, cmd);

    return 0;
}