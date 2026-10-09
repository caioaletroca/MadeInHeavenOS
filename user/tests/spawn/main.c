#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <spawn.h>
#include <sys/wait.h>
#include <errno.h>
#include <test.h>

/*
 * posix_spawn and waitpid. The test spawns itself: argv[1] picks what the
 * child does, so no other program has to be on the boot modules.
 */

static char *self; // argv[0]: this program's path

// Spawn this program with mode (and arg, may be NULL); returns the pid or -1
static pid_t child_spawn(const char *mode, const char *arg, char *const envp[])
{
    char *argv[] = {self, (char *)mode, (char *)arg, NULL};
    pid_t pid = -1;
    int error = posix_spawn(&pid, self, NULL, NULL, argv, envp);
    if (error != 0)
    {
        printf("spawn %s failed: %d\n", mode, error);
        return -1;
    }
    return pid;
}

// Spawn with mode/arg, wait for it, return the raw wait status (-1 if spawn or wait failed)
static int child_run(const char *mode, const char *arg, char *const envp[])
{
    pid_t pid = child_spawn(mode, arg, envp);
    if (pid <= 0)
        return -1;

    int status = -1;
    if (waitpid(pid, &status, 0) != pid)
        return -1;
    return status;
}

/*
 * Child side. Each mode returns its exit status.
 */
static int child_main(int argc, char **argv, char **envp)
{
    const char *mode = argv[1];

    if (strcmp(mode, "exit") == 0)
        return atoi(argv[2]);

    if (strcmp(mode, "segv") == 0)
    {
        *(volatile int *)0 = 1;
        return 0; // not reached
    }

    // argv and envp exactly as the parent passed them
    if (strcmp(mode, "args") == 0)
    {
        CHECK(argc == 3);
        CHECK(strcmp(argv[0], self) == 0);
        CHECK(strcmp(argv[2], "two words") == 0);
        CHECK(argv[3] == NULL);
        CHECK(envp[0] != NULL && strcmp(envp[0], "A=1") == 0);
        CHECK(envp[1] != NULL && strcmp(envp[1], "B=2") == 0);
        CHECK(envp[2] == NULL);
        CHECK(environ == envp);
        return test_status();
    }

    // envp NULL in posix_spawn: an empty environment
    if (strcmp(mode, "noenv") == 0)
    {
        CHECK(envp[0] == NULL);
        return test_status();
    }

    // The parent closed fd 0: inherited fds keep their numbers, the hole stays a hole
    if (strcmp(mode, "fds") == 0)
    {
        char byte;
        errno = 0;
        CHECK(read(0, &byte, 1) == -1 && errno == EBADF);
        CHECK(write(1, "", 0) == 0);
        CHECK(write(2, "", 0) == 0);
        return test_status();
    }

    // Spawns a grandchild and exits without waiting: the kernel must drop it cleanly
    if (strcmp(mode, "orphan") == 0)
    {
        char *argv_grandchild[] = {self, "exit", "0", NULL};
        pid_t pid;
        return posix_spawn(&pid, self, NULL, NULL, argv_grandchild, NULL);
    }

    printf("unknown child mode %s\n", mode);
    return 99;
}

static void test_status_encoding(void)
{
    int status = child_run("exit", "7", NULL);
    CHECK(WIFEXITED(status) && !WIFSIGNALED(status));
    CHECK(WEXITSTATUS(status) == 7);

    // Only the low 8 bits of the exit value reach the parent
    status = child_run("exit", "300", NULL);
    CHECK(WIFEXITED(status) && WEXITSTATUS(status) == 300 % 256);

    // Killed by a fault: the signal, not 128 + signal as an exit value
    status = child_run("segv", NULL, NULL);
    CHECK(WIFSIGNALED(status) && !WIFEXITED(status));
    CHECK(WTERMSIG(status) == 11); // SIGSEGV
}

static void test_arguments(void)
{
    char *envp[] = {"A=1", "B=2", NULL};
    int status = child_run("args", "two words", envp);
    CHECK(WIFEXITED(status) && WEXITSTATUS(status) == 0);

    status = child_run("noenv", NULL, NULL);
    CHECK(WIFEXITED(status) && WEXITSTATUS(status) == 0);
}

static void test_wait_order(void)
{
    // Two children alive at once, waited for in reverse order
    pid_t first = child_spawn("exit", "1", NULL);
    pid_t second = child_spawn("exit", "2", NULL);
    CHECK(first > 0 && second > 0 && first != second);

    int status;
    CHECK(waitpid(second, &status, 0) == second && WEXITSTATUS(status) == 2);
    CHECK(waitpid(first, &status, 0) == first && WEXITSTATUS(status) == 1);

    // Waited for already: no longer a child
    errno = 0;
    CHECK(waitpid(first, &status, 0) == -1 && errno == ECHILD);

    // A status pointer is optional
    pid_t pid = child_spawn("exit", "0", NULL);
    CHECK(waitpid(pid, NULL, 0) == pid);
}

static void test_many(void)
{
    // References are dropped every time: a leak or a double release shows up here
    int ok = 1;
    for (int i = 0; i < 20; i++)
    {
        char value[4] = {'0' + i / 10, '0' + i % 10, '\0'};
        int status = child_run("exit", value, NULL);
        if (!WIFEXITED(status) || WEXITSTATUS(status) != i)
            ok = 0;
    }
    CHECK(ok);
}

static void test_errors(void)
{
    pid_t pid = 0;
    char *argv[] = {"x", NULL};

    CHECK(posix_spawn(&pid, "/usr/tests/missing", NULL, NULL, argv, NULL) == ENOENT);
    CHECK(posix_spawn(&pid, self, NULL, NULL, (char *const *)1, NULL) == EFAULT);
    CHECK(posix_spawn(&pid, (const char *)1, NULL, NULL, argv, NULL) == EFAULT);

    // More entries than the kernel copies (64)
    char *many[70];
    for (int i = 0; i < 69; i++)
        many[i] = "a";
    many[69] = NULL;
    CHECK(posix_spawn(&pid, self, NULL, NULL, many, NULL) == E2BIG);

    // A path longer than any module name
    char long_path[100];
    memset(long_path, 'p', sizeof(long_path) - 1);
    long_path[sizeof(long_path) - 1] = '\0';
    CHECK(posix_spawn(&pid, long_path, NULL, NULL, argv, NULL) == E2BIG);

    // File actions and attributes are not supported yet
    int dummy;
    CHECK(posix_spawn(&pid, self, (const posix_spawn_file_actions_t *)&dummy, NULL, argv, NULL) == ENOSYS);

    int status;
    errno = 0;
    CHECK(waitpid(1, &status, 0) == -1 && errno == ECHILD); // not our child
    errno = 0;
    CHECK(waitpid(-1, &status, 0) == -1 && errno == ECHILD); // "any child": not yet
    errno = 0;
    CHECK(waitpid(1, &status, 1) == -1 && errno == EINVAL); // WNOHANG: not yet
}

static void test_orphan(void)
{
    // The child returns posix_spawn's result: 0 if the grandchild started
    int status = child_run("orphan", NULL, NULL);
    CHECK(WIFEXITED(status) && WEXITSTATUS(status) == 0);
}

static void test_fds(void)
{
    // Last: the parent gives up its stdin
    CHECK(close(0) == 0);
    int status = child_run("fds", NULL, NULL);
    CHECK(WIFEXITED(status) && WEXITSTATUS(status) == 0);
}

int main(int argc, char **argv, char **envp)
{
    self = argv[0];

    if (argc > 1)
        return child_main(argc, argv, envp);

    test_status_encoding();
    test_arguments();
    test_wait_order();
    test_many();
    test_errors();
    test_orphan();
    test_fds();

    printf("spawn: %d failure(s)\n", test_failures);
    return test_status();
}
