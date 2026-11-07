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
 * @brief Determines the exit status of a program based on its wait status.
 *
 * @param name The name of the program.
 * @param status The wait status.
 * @return The exit status of the program.
 */
static int program_status(const char *name, int status)
{
    if (WIFSIGNALED(status))
    {
        // A reader that left is normal in a pipeline, and whoever pressed Ctrl+C knows
        if (WTERMSIG(status) != SIGPIPE && WTERMSIG(status) != SIGINT)
            fprintf(stderr, "sh: %s: killed by signal %d\n", name, WTERMSIG(status));
        return 128 + WTERMSIG(status);
    }

    return WEXITSTATUS(status);
}

int job_wait(shell_t *sh, job_t *job)
{
    int options = sh->interactive ? WUNTRACED : 0;

    for (; job->waited < job->count; job->waited++)
    {
        int status;
        int result = 1;
        pid_t pid = job->pids[job->waited];

        if (waitpid(pid, &status, options) != pid)
            fprintf(stderr, "sh: %s: waitpid failed\n", job->command);
        else if (WIFSTOPPED(status))
        {
            sh->status = 128 + WSTOPSIG(status);
            return 1;
        }
        else
            result = program_status(job->command, status);

        if (job->waited == job->stages - 1)
            sh->status = result;
    }

    return 0;
}

/**
 * @brief Keeps a stopped job in the job list.
 *
 * @param sh The shell instance.
 * @param job The job to keep.
 */
static void job_keep(shell_t *sh, job_t *job)
{
    if (sh->job_count == JOBS_MAX)
    {
        fprintf(stderr, "sh: too many stopped jobs, ending: %s\n", job->command);
        kill(-job->group, SIGKILL);
        job_wait(sh, job);
        return;
    }

    sh->jobs[sh->job_count++] = *job;
    fprintf(stderr, "[%d]  Stopped  %s\n", sh->job_count, job->command);
}

int job_resume(shell_t *sh, job_t *job)
{
    fprintf(stderr, "%s\n", job->command);

    tcsetpgrp(0, job->group);
    kill(-job->group, SIGCONT);

    int stopped = job_wait(sh, job);
    tcsetpgrp(0, sh->group);

    if (stopped)
        job_keep(sh, job);

    return sh->status;
}

void jobs_hang_up(shell_t *sh)
{
    for (int i = 0; i < sh->job_count; i++)
    {
        kill(-sh->jobs[i].group, SIGHUP);
        kill(-sh->jobs[i].group, SIGCONT);
    }
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
    signal(SIGTSTP, SIG_DFL);
    signal(SIGTTIN, SIG_DFL);
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
    job_t job = {.stages = cmd->count};
    strlcpy(job.command, cmd->command, sizeof(job.command));
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
            sh->job_count = 0;

            if (sh->interactive)
                stage_job_setup(job.group);

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

        job.pids[job.count++] = pid;

        // Here too, whichever side runs first. A child already past exec answers EACCES, and it is done
        if (sh->interactive)
        {
            if (job.group == 0)
                job.group = pid;

            setpgid(pid, job.group);
            tcsetpgrp(0, job.group);
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

    sh->status = 1;

    int stopped = job_wait(sh, &job);

    if (sh->interactive)
        tcsetpgrp(0, sh->group);

    if (stopped)
        job_keep(sh, &job);
}

int command_run(shell_t *sh, command_t *cmd)
{
    // Alone, a builtin runs in the shell itself: cd and exit must change this process
    if (cmd->count > 1 || !builtin_run(sh, &cmd->stages[0]))
        pipeline_run(sh, cmd);

    return 0;
}