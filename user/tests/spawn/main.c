#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <spawn.h>
#include <sys/wait.h>
#include <errno.h>
#include <fcntl.h>
#include <test.h>

/*
 * posix_spawn, waitpid, wait, getpid and getppid. The test spawns itself: argv[1] picks what the
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

    // A privileged instruction at CPL 3: the kill goes through the general protection fault path
    if (strcmp(mode, "hlt") == 0)
    {
        __asm__ volatile("hlt");
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

    // getppid is the parent's pid (argv[2]); the parent compares the status with our pid
    if (strcmp(mode, "pids") == 0)
    {
        if (getppid() != atoi(argv[2]))
            return 255;
        return getpid() % 200;
    }

    // Spawns a grandchild and exits at once: init must adopt the grandchild
    if (strcmp(mode, "adopt") == 0)
    {
        char *argv_grandchild[] = {self, "adopted", NULL};
        pid_t pid;
        return posix_spawn(&pid, self, NULL, NULL, argv_grandchild, NULL);
    }

    // Nobody can wait for an orphan: a missing adoption shows as this FAIL line
    if (strcmp(mode, "adopted") == 0)
    {
        for (long i = 0; i < 1000000; i++)
            if (getppid() == 1)
                return 0;

        printf("FAIL: orphan never adopted by init (getppid %d)\n", getppid());
        return 1;
    }

    // Reads 10 bytes from an inherited fd (argv[2]): the offset is shared with the parent
    if (strcmp(mode, "readfd") == 0)
    {
        char buffer[10];
        return read(atoi(argv[2]), buffer, sizeof(buffer)) == sizeof(buffer) ? 0 : 1;
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

    // A #GP (hlt) kills with SIGSEGV too, as on Linux
    status = child_run("hlt", NULL, NULL);
    CHECK(WIFSIGNALED(status) && WTERMSIG(status) == 11);
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

static void test_wait_any(void)
{
    int status;

    // No children at all
    errno = 0;
    CHECK(wait(&status) == -1 && errno == ECHILD);

    // Most likely still running when wait is called: it must sleep until the exit
    pid_t pid = child_spawn("exit", "5", NULL);
    CHECK(wait(&status) == pid && WIFEXITED(status) && WEXITSTATUS(status) == 5);

    // Three at once: each comes back exactly once, with its own status
    const char *values[] = {"3", "4", "6"};
    pid_t pids[3];
    for (int i = 0; i < 3; i++)
        pids[i] = child_spawn("exit", values[i], NULL);

    int seen[3] = {0, 0, 0};
    for (int n = 0; n < 3; n++)
    {
        pid_t got = waitpid(-1, &status, 0);
        int match = -1;
        for (int i = 0; i < 3; i++)
            if (got == pids[i])
                match = i;

        CHECK(match >= 0);
        if (match >= 0)
        {
            CHECK(!seen[match]);
            CHECK(WIFEXITED(status) && WEXITSTATUS(status) == atoi(values[match]));
            seen[match] = 1;
        }
    }
    errno = 0;
    CHECK(wait(NULL) == -1 && errno == ECHILD);

    // Waiting for one by pid leaves the other for "any child"
    pid_t first = child_spawn("exit", "1", NULL);
    pid_t second = child_spawn("exit", "2", NULL);
    CHECK(waitpid(second, &status, 0) == second && WEXITSTATUS(status) == 2);
    CHECK(wait(&status) == first && WEXITSTATUS(status) == 1);

    // Process groups (0, < -1) do not exist yet: ECHILD even with a child alive
    pid = child_spawn("exit", "0", NULL);
    errno = 0;
    CHECK(waitpid(0, &status, 0) == -1 && errno == ECHILD);
    errno = 0;
    CHECK(waitpid(-2, &status, 0) == -1 && errno == ECHILD);
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
    CHECK(posix_spawn(&pid, "/usr/tests", NULL, NULL, argv, NULL) == EACCES); // a directory
    CHECK(posix_spawn(&pid, "/usr/tests/hello/", NULL, NULL, argv, NULL) == ENOTDIR);
    CHECK(posix_spawn(&pid, "", NULL, NULL, argv, NULL) == ENOENT);

    // Relative: found from the current directory (/), and it runs (hello exits with 42)
    int hello_status;
    CHECK(posix_spawn(&pid, "usr/tests/hello", NULL, NULL, argv, NULL) == 0);
    CHECK(waitpid(pid, &hello_status, 0) == pid && WIFEXITED(hello_status) && WEXITSTATUS(hello_status) == 42);
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
    CHECK(waitpid(-1, &status, 0) == -1 && errno == ECHILD); // no children left
    errno = 0;
    CHECK(waitpid(1, &status, 1) == -1 && errno == EINVAL); // WNOHANG: not yet
}

static void test_orphan(void)
{
    // The child returns posix_spawn's result: 0 if the grandchild started
    int status = child_run("orphan", NULL, NULL);
    CHECK(WIFEXITED(status) && WEXITSTATUS(status) == 0);

    // The grandchild is not ours to wait for
    errno = 0;
    CHECK(wait(NULL) == -1 && errno == ECHILD);
}

static void test_pids(void)
{
    // Spawned by the runner, not by the kernel: neither init nor parentless
    CHECK(getpid() > 1);
    CHECK(getppid() > 1 && getppid() != getpid());

    char self_pid[12];
    snprintf(self_pid, sizeof(self_pid), "%d", getpid());
    pid_t pid = child_spawn("pids", self_pid, NULL);
    int status;
    CHECK(waitpid(pid, &status, 0) == pid);
    CHECK(WIFEXITED(status) && WEXITSTATUS(status) == pid % 200);

    // The grandchild checks its own adoption (see "adopted")
    status = child_run("adopt", NULL, NULL);
    CHECK(WIFEXITED(status) && WEXITSTATUS(status) == 0);
}

static void test_shared_offset(void)
{
    // An inherited fd is the same open file: a child's read moves the parent's offset (POSIX)
    int fd = open("/usr/tests/hello", O_RDONLY);
    CHECK(fd >= 0);

    char number[4];
    snprintf(number, sizeof(number), "%d", fd);
    int status = child_run("readfd", number, NULL);
    CHECK(WIFEXITED(status) && WEXITSTATUS(status) == 0);
    CHECK(lseek(fd, 0, SEEK_CUR) == 10);

    close(fd);
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
    test_wait_any();
    test_many();
    test_errors();
    test_orphan();
    test_pids();
    test_shared_offset();
    test_fds();

    return test_status();
}
