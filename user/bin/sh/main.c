#include <stdio.h>
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

        history_add(&sh.history, cmd.command);

        if (command_parse(&cmd) == READ_NOTHING)
            continue;

        command_run(&sh, &cmd);
    }

    return sh.status;
}