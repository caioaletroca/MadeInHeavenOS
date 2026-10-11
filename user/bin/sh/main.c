#include <stdio.h>
#include <string.h>
#include "sh.h"

static void print_header(void)
{
    printf("Welcome to MiHos Shell\n");
    printf("=====================\n\n");
}

static void print_prompt(void)
{
    printf("mihos>");
}

int main(void)
{
    shell_t sh = {.running = 1};
    command_t cmd;

    print_header();

    while (sh.running)
    {
        print_prompt();

        if (command_read(&cmd) == READ_EOF)
            break;

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

        if (command_parse(&sh, &cmd) == READ_NOTHING)
            continue;

        command_run(&sh, &cmd);
    }

    return sh.status;
}