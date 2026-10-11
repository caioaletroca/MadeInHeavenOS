#include <stdio.h>
#include <string.h>
#include <spawn.h>
#include <sys/reboot.h>
#include <sys/wait.h>
#include <errno.h>

#define CRASHES_MAX 5 // a command killed this many times in a row is given up

/*
 * Start a new process with the given arguments and environment.
 * Returns the PID of the new process, or -1 on error.
 */
static pid_t start(char *argv[], char *envp[])
{
    pid_t pid;
    int error = posix_spawn(&pid, argv[0], NULL, NULL, argv, envp);
    if (error != 0)
    {
        printf("init: cannot start %s: error %d\n", argv[0], error);
        return -1;
    }

    return pid;
}

/*
 * Halt the system by calling the reboot system call with RB_HALT_SYSTEM.
 * This function does not return.
 */
__attribute__((noreturn)) static void halt(void)
{
    fflush(stdout);
    reboot(RB_HALT_SYSTEM);

    printf("init: reboot failed: errno %d\n", errno);
    fflush(stdout);

    for (;;)
        ;
}

// once: run the command, report how it ended, halt
__attribute__((noreturn)) static void run_once(char *command[], char *envp[])
{
    pid_t pid = start(command, envp);
    if (pid < 0)
        halt();

    int status;
    pid_t got;
    while ((got = wait(&status)) != pid) // anything else is a reaped orphan
    {
        if (got < 0)
        {
            printf("init: wait failed\n");
            halt();
        }
    }

    if (WIFSIGNALED(status))
        printf("init: %s killed by signal %d\n", command[0], WTERMSIG(status));
    else
        printf("init: %s exited with status %d\n", command[0], WEXITSTATUS(status));
    halt();
}

// respawn: like a service manager with Restart=always, giving up on a crash loop
__attribute__((noreturn)) static void run_respawn(char *command[], char *envp[])
{
    int crashes = 0;
    pid_t pid = start(command, envp);
    if (pid < 0)
        halt();

    for (;;)
    {
        int status;
        pid_t got;
        while ((got = wait(&status)) != pid) // anything else is a reaped orphan
        {
            if (got < 0)
            {
                printf("init: wait failed\n");
                halt();
            }
        }

        if (WIFSIGNALED(status))
        {
            printf("init: %s killed by signal %d\n", command[0], WTERMSIG(status));
            if (++crashes == CRASHES_MAX)
            {
                printf("init: %s keeps crashing, giving up\n", command[0]);
                halt();
            }
        }
        else
        {
            crashes = 0; // exit and Ctrl+D are normal: a fresh shell, no message
        }

        pid = start(command, envp);
        if (pid < 0)
            halt();
    }
}

/*
 * /sbin/init, pid 1: the only program the kernel starts, from its GRUB
 * module line. It never exits (the kernel panics if it does):
 *   /sbin/init once <command...>      run the command, then halt the system
 *   /sbin/init respawn <command...>   run it again every time it ends
 * Every child is reaped through wait(-1), so orphans the kernel hands to
 * init later need no extra code.
 */
int main(int argc, char *argv[], char *envp[])
{
    if (argc >= 3 && strcmp(argv[1], "once") == 0)
        run_once(&argv[2], envp);
    if (argc >= 3 && strcmp(argv[1], "respawn") == 0)
        run_respawn(&argv[2], envp);

    printf("init: usage: /sbin/init once|respawn <command> [arguments]\n");
    halt();
}