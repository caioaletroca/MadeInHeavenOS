#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "sh.h"

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

int main(void)
{
    shell_t sh = {.running = 1, .interactive = isatty(0) && isatty(2)};
    command_t cmd;

    if (sh.interactive)
        print_header();

    while (sh.running)
    {
        if (sh.interactive)
            print_prompt();

        if (command_read(&cmd) == READ_EOF)
        {
            if (sh.interactive)
                fputc('\n', stderr);
            break;
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

    return sh.status;
}