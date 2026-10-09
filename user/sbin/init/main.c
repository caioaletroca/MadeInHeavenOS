#include <stdio.h>
#include <spawn.h>
#include <sys/wait.h>

/*
 * /sbin/init, pid 1: the only program the kernel starts. Its arguments are
 * one command (grub.cfg: "/sbin/init /usr/tests/run"); init runs it, waits
 * and exits with its status. The kernel then halts.
 * TODO: init must never exit: once there is a shell, run it and restart it.
 */
int main(int argc, char *argv[], char *envp[])
{
    if (argc < 2)
    {
        printf("init: nothing to run\n");
        return 1;
    }

    pid_t pid;
    int error = posix_spawn(&pid, argv[1], NULL, NULL, &argv[1], envp);
    if (error != 0)
    {
        printf("init: cannot start %s: error %d\n", argv[1], error);
        return 1;
    }

    int status;
    if (waitpid(pid, &status, 0) != pid)
    {
        printf("init: waitpid failed\n");
        return 1;
    }

    if (WIFSIGNALED(status))
    {
        printf("init: %s killed by signal %d\n", argv[1], WTERMSIG(status));
        return 128 + WTERMSIG(status);
    }
    return WEXITSTATUS(status);
}