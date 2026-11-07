#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include "sh.h"

static volatile sig_atomic_t interrupted;

static void print_header(void)
{
    printf("Welcome to MiHos Shell\n");
    printf("=====================\n\n");
}

static void print_prompt(void)
{
    char cwd[CWD_MAX];
    if (getcwd(cwd, sizeof(cwd)) == NULL)
        strlcpy(cwd, "?", sizeof(cwd));

    fprintf(stderr, "mihos:%s> ", cwd);
}

// Being called is what ends the read, so the prompt comes back
static void on_prompt_key(int signal)
{
    if (signal == SIGINT)
        interrupted = 1;
}

static void terminal_setup(shell_t *sh)
{
    sh->group = getpgrp();
    tcsetpgrp(0, sh->group);

    // No SA_RESTART: read must end. Ctrl+Z too: the shell itself never stops
    struct sigaction keys = {.sa_handler = on_prompt_key};
    sigaction(SIGINT, &keys, NULL);
    sigaction(SIGTSTP, &keys, NULL);

    signal(SIGQUIT, SIG_IGN);
    signal(SIGTTIN, SIG_IGN);

    // Taking the terminal back from a pipeline would stop the shell
    signal(SIGTTOU, SIG_IGN);
}

int main(void)
{
    shell_t sh = {.running = 1, .interactive = isatty(0) && isatty(2)};
    command_t cmd;

    if (sh.interactive)
    {
        terminal_setup(&sh);
        print_header();
    }

    while (sh.running)
    {
        if (sh.interactive)
            print_prompt();

        read_result_t input = command_read(&cmd);
        if (input == READ_EOF)
        {
            if (sh.interactive)
                fputc('\n', stderr);
            break;
        }

        // Ctrl+C at the prompt: 128 + SIGINT, as bash. Ctrl+Z changes nothing
        if (input == READ_NOTHING)
        {
            if (interrupted)
                sh.status = 130;

            interrupted = 0;
            continue;
        }

        // History only for someone typing (bash: off in scripts)
        if (sh.interactive)
        {
            char expanded[COMMAND_MAX];
            int ret = history_expand(&sh.history, cmd.command, expanded, sizeof(expanded));
            if (ret < 0)
                continue;

            if (ret == 1)
            {
                strlcpy(cmd.command, expanded, sizeof(cmd.command));
                printf("%s\n", cmd.command);
            }

            history_add(&sh.history, expanded);
        }

        if (command_parse(&sh, &cmd) == READ_NOTHING)
            continue;

        command_run(&sh, &cmd);

        // Into a pipe or a file stdout is fully buffered: what this command printed must be
        // out before the next program writes its own
        fflush(stdout);
    }

    jobs_hang_up(&sh);
    return sh.status;
}