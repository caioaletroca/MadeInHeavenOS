#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/reboot.h>
#include <sys/wait.h>
#include <sys/ioctl.h>
#include <errno.h>

#define CRASHES_MAX 5         // a command killed this many times in a row is given up
#define EXIT_CANNOT_START 127 // exec failed in the child, the shell's "command not found"

/*
 * Start a command in a session of its own, owning the console: what getty does
 * for a login. Fork and exec, not spawn, since both must happen in the child.
 * Returns the PID of the new process, or -1 on error.
 */
static pid_t start(char *argv[], char *envp[])
{
    fflush(stdout);

    pid_t pid = fork();
    if (pid < 0)
    {
        printf("init: cannot start %s: error %d\n", argv[0], errno);
        return -1;
    }
    if (pid == 0)
    {
        setsid();
        ioctl(0, TIOCSCTTY, NULL); // set controlling terminal
        execve(argv[0], argv, envp);

        printf("init: cannot start %s: error %d\n", argv[0], errno);
        fflush(stdout);
        _exit(EXIT_CANNOT_START);
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
            printf("init: %s killed by signal %d\n", command[0], WTERMSIG(status));

        // A program that cannot start would otherwise be started again forever
        if (WIFSIGNALED(status) || WEXITSTATUS(status) == EXIT_CANNOT_START)
        {
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