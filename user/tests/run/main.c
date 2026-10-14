#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <spawn.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/wait.h>

/*
 * Test runner: spawns every test of a set in order and compares its exit
 * status with the expected one. /sbin/init starts it from grub.cfg:
 *   /usr/tests/run               the automatic tests
 *   /usr/tests/run interactive   the tests that need typing
 * Exits with the number of failed tests.
 *
 * The automatic set is every program in TESTS_DIR except this runner and the
 * interactive tests, found with readdir and run in name order, so a new test
 * needs no line here. Tests expect status 0 unless `special` says otherwise.
 */

#define TESTS_DIR "/usr/tests"
#define TESTS_MAX 64
#define PATH_MAX_TEST 288 // TESTS_DIR + '/' + a 255-byte name + '\0'

typedef struct expectation
{
    const char *name;
    int expected; // exit status, 0..255
} expectation_t;

// Tests that do not exit with 0
static const expectation_t special[] = {
    {"hello", 42},         // returns a .data 42 plus a .bss 0
    {"malloc_abort", 134}, // double free → abort: 128 + SIGABRT
    {"cxx", 42},           // its last global destructor exits with 42
    {NULL, 0},
};

// Tests that need someone typing: only in the interactive set
static const char *const interactive[] = {
    "stdin_interactive",
    NULL,
};

static int name_in(const char *name, const char *const list[])
{
    for (size_t i = 0; list[i] != NULL; i++)
        if (strcmp(name, list[i]) == 0)
            return 1;
    return 0;
}

static int expected_status(const char *name)
{
    for (const expectation_t *e = special; e->name != NULL; e++)
        if (strcmp(name, e->name) == 0)
            return e->expected;
    return 0;
}

// Spawn one test and wait for it; returns 1 if it passed
static int test_run(const char *name, int expected)
{
    char path[PATH_MAX_TEST];
    snprintf(path, sizeof(path), "%s/%s", TESTS_DIR, name);

    char *argv[] = {path, NULL};
    pid_t pid;

    int error = posix_spawn(&pid, path, NULL, NULL, argv, environ);
    if (error != 0)
    {
        printf("test %s: FAIL (could not start: %d)\n", path, error);
        return 0;
    }

    int status;
    if (waitpid(pid, &status, 0) != pid)
    {
        printf("test %s: FAIL (waitpid failed)\n", path);
        return 0;
    }

    if (WIFSIGNALED(status))
    {
        printf("test %s: FAIL (signal %d, expected status %d)\n", path, WTERMSIG(status), expected);
        return 0;
    }
    if (WEXITSTATUS(status) != expected)
    {
        printf("test %s: FAIL (status %d, expected %d)\n", path, WEXITSTATUS(status), expected);
        return 0;
    }

    printf("test %s: ok\n", path);
    return 1;
}

/*
 * Collects the automatic tests' names into names[], sorted. The directory is
 * closed before any test runs: spawned tests inherit fds, and the files and
 * dirs tests expect fd 3 to be free. Returns the count, or -1 on an error.
 */
static int tests_find(char *names[], int max)
{
    DIR *dir = opendir(TESTS_DIR);
    if (dir == NULL)
    {
        printf("tests: FAIL (cannot open %s: %s)\n", TESTS_DIR, strerror(errno));
        return -1;
    }

    int count = 0;
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL)
    {
        const char *name = entry->d_name;
        if (name[0] == '.' || entry->d_type != DT_REG || strcmp(name, "run") == 0 || name_in(name, interactive))
            continue;

        if (count == max)
        {
            printf("tests: FAIL (more than %d tests)\n", max);
            closedir(dir);
            return -1;
        }

        names[count] = strdup(name);
        if (names[count] == NULL)
        {
            printf("tests: FAIL (out of memory)\n");
            closedir(dir);
            return -1;
        }

        // Insertion sort: the order must not depend on the file system
        for (int i = count; i > 0 && strcmp(names[i - 1], names[i]) > 0; i--)
        {
            char *swap = names[i - 1];
            names[i - 1] = names[i];
            names[i] = swap;
        }
        count++;
    }

    closedir(dir);
    return count;
}

int main(int argc, char **argv)
{
    int passed = 0;
    int failed = 0;

    if (argc > 1 && strcmp(argv[1], "interactive") == 0)
    {
        for (size_t i = 0; interactive[i] != NULL; i++)
        {
            if (test_run(interactive[i], expected_status(interactive[i])))
                passed++;
            else
                failed++;
        }

        printf("tests: %d passed, %d failed\n", passed, failed);
        return failed;
    }

    char *names[TESTS_MAX];
    int count = tests_find(names, TESTS_MAX);

    // Finding nothing is a failure: "0 passed, 0 failed" must never look like a pass
    if (count <= 0)
    {
        if (count == 0)
            printf("tests: FAIL (no tests in %s)\n", TESTS_DIR);
        return 1;
    }

    // A special status whose test is gone (renamed, removed) would otherwise vanish quietly
    for (const expectation_t *e = special; e->name != NULL; e++)
    {
        int found = 0;
        for (int i = 0; i < count; i++)
            found = found || strcmp(names[i], e->name) == 0;
        if (!found)
        {
            printf("test %s/%s: FAIL (expected status %d, but not found)\n", TESTS_DIR, e->name, e->expected);
            failed++;
        }
    }

    for (int i = 0; i < count; i++)
    {
        if (test_run(names[i], expected_status(names[i])))
            passed++;
        else
            failed++;

        free(names[i]);
    }

    printf("tests: %d passed, %d failed\n", passed, failed);
    return failed;
}
