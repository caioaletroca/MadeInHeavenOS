#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include "sh.h"

static void print_header(void)
{
    printf("Welcome to MiHos Shell\n");
    printf("=====================\n\n");
}

static void terminal_setup(shell_t *sh)
{
    sh->group = getpgrp();
    tcsetpgrp(0, sh->group);
    line_editor_init(&sh->editor, 0, 2);

    // The keys reach only the foreground group, and at a prompt they send no signal at all
    signal(SIGINT, SIG_IGN);
    signal(SIGTSTP, SIG_IGN);
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
        read_result_t input = command_read(&sh, &cmd);
        if (input == READ_EOF)
        {
            if (sh.interactive)
                fputc('\n', stderr);
            break;
        }

        if (input == READ_NOTHING)
            continue;

        // History only for someone typing (bash: off in scripts)
        if (sh.interactive)
        {
            char expanded[COMMAND_MAX];
            int ret = history_expand(&sh.editor.history, cmd.command, expanded, sizeof(expanded));
            if (ret < 0)
                continue;

            if (ret == 1)
            {
                strlcpy(cmd.command, expanded, sizeof(cmd.command));
                printf("%s\n", cmd.command);
            }

            line_history_add(&sh.editor.history, expanded);
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