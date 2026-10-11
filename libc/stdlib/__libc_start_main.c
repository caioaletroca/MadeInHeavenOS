#include <stdlib.h>
#include <unistd.h>
#include <libc.h>

int main(int argc, char **argv, char **envp);

/*
 * Called by crt0's _start with the initial stack's values: sets environ, runs
 * the global constructors (C++), then main, and exits with its result.
 */
__attribute__((noreturn)) void __libc_start_main(int argc, char **argv, char **envp)
{
    environ = envp; // first: constructors may call getenv

    for (size_t i = 0; i < (size_t)(__init_array_end - __init_array_start); i++)
        __init_array_start[i]();

    exit(main(argc, argv, envp));
}