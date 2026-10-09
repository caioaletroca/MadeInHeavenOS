#include <string.h>
#include <stdio.h>
#include <spawn.h>
#include <unistd.h>
#include <sys/wait.h>

/*
 * Test runner: spawns every test of a set in order and compares its exit
 * status with the expected one. /sbin/init starts it from grub.cfg:
 *   /usr/tests/run               the automatic tests
 *   /usr/tests/run interactive   the tests that need typing
 * Exits with the number of failed tests. Every program listed here must
 * also be a module in grub.cfg (until the initramfs, phase B).
 */

typedef struct test
{
    const char *path;
    int expected; // exit status, 0..255
} test_t;

static const test_t automatic[] = {
    {"/usr/tests/hello", 42},
    {"/usr/tests/stdio", 0},
    {"/usr/tests/brk", 0},
    {"/usr/tests/malloc", 0},
    {"/usr/tests/malloc_abort", 134},
    {"/usr/tests/stdin", 0},
    {"/usr/tests/args", 0},
    {"/usr/tests/spawn", 0},
    {NULL, 0},
};

static const test_t interactive[] = {
    {"/usr/tests/stdin_interactive", 0},
    {NULL, 0},
};

// Spawn one test and wait for it; returns 1 if it passed
static int test_run(const test_t *test)
{
    char *argv[] = {(char *)test->path, NULL};
    pid_t pid;

    int error = posix_spawn(&pid, test->path, NULL, NULL, argv, environ);
    if (error != 0)
    {
        printf("test %s: FAIL (could not start: %d)\n", test->path, error);
        return 0;
    }

    int status;
    if (waitpid(pid, &status, 0) != pid)
    {
        printf("test %s: FAIL (waitpid failed)\n", test->path);
        return 0;
    }

    if (WIFSIGNALED(status))
    {
        printf("test %s: FAIL (signal %d, expected status %d)\n", test->path, WTERMSIG(status), test->expected);
        return 0;
    }
    if (WEXITSTATUS(status) != test->expected)
    {
        printf("test %s: FAIL (status %d, expected %d)\n", test->path, WEXITSTATUS(status), test->expected);
        return 0;
    }

    printf("test %s: ok\n", test->path);
    return 1;
}

int main(int argc, char **argv)
{
    const test_t *tests = automatic;
    if (argc > 1 && strcmp(argv[1], "interactive") == 0)
        tests = interactive;

    int passed = 0;
    int failed = 0;
    for (const test_t *test = tests; test->path != NULL; test++)
    {
        if (test_run(test))
            passed++;
        else
            failed++;
    }

    printf("tests: %d passed, %d failed\n", passed, failed);
    return failed;
}
