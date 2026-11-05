#include <sys/wait.h>
#include <fcntl.h>
#include <unistd.h>
#include <spawn.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <test.h>

/*
 * execve: the same process, a new program. Most cases fork a child that
 * prepares something and execs this test again with a mode in argv[1]; the new
 * image checks what it was handed and exits with its failed checks, so the
 * parent's wait sees them. What stays: pid, parent, children, directory, open
 * fds and their offsets. What goes: memory, the break, fds marked FD_CLOEXEC.
 * A failed exec changes nothing. Then fcntl's close-on-exec flag, execv and
 * execvp.
 */

#define HELLO "/usr/tests/hello"
#define GROWTH (1024 * 1024) // the break moved far enough to see it come back
#define CHAIN 50             // execs in a row in one process
#define NUMBER_MAX 16
#define EXEC_FAILED 99 // a child's status if its exec came back

static char *self;
static int global = 10;
static int zeroed; // in .bss
static char *test_environ[] = {"EXEC_TEST=yes", NULL};

// Execs this test in mode with up to two number arguments; returns only on failure
static void exec_self(const char *mode, long first, long second)
{
    char a[NUMBER_MAX], b[NUMBER_MAX];
    snprintf(a, sizeof(a), "%ld", first);
    snprintf(b, sizeof(b), "%ld", second);

    char *argv[] = {self, (char *)mode, a, b, NULL};
    execve(self, argv, test_environ);
}

static pid_t fork_flushed(void)
{
    fflush(stdout);
    return fork();
}

// Waits for pid and checks it exited with status 0
static void child_passed(pid_t pid)
{
    int status = -1;
    CHECK(waitpid(pid, &status, 0) == pid);
    CHECK(WIFEXITED(status));
    CHECK(WEXITSTATUS(status) == 0);
}

/*
 * The new images. Each mode returns its exit status.
 */
static int child_main(int argc, char **argv)
{
    const char *mode = argv[1];
    long first = argc > 2 ? atoi(argv[2]) : 0;
    long second = argc > 3 ? atoi(argv[3]) : 0;

    // Same process, new memory
    if (strcmp(mode, "fresh") == 0)
    {
        CHECK(argc == 4 && argv[4] == NULL);
        CHECK(strcmp(argv[0], self) == 0);
        CHECK(getpid() == first);
        CHECK(getppid() == second);
        CHECK(global == 10 && zeroed == 0);
        CHECK(strcmp(getenv("EXEC_TEST"), "yes") == 0);
        CHECK(getenv("TEST_ROOT") == NULL); // the old environment is gone
    }
    // The heap starts over, below where the old program left its break
    else if (strcmp(mode, "break") == 0)
    {
        CHECK((char *)sbrk(0) < (char *)(uintptr_t)first);
    }
    // first: a pipe end kept across the exec
    else if (strcmp(mode, "write") == 0)
    {
        CHECK(write((int)first, "from the new program", 20) == 20);
    }
    // first: an fd, read 4 bytes into. second: an fd marked close-on-exec
    else if (strcmp(mode, "files") == 0)
    {
        CHECK(lseek((int)first, 0, SEEK_CUR) == 4);
        char byte;
        errno = 0;
        CHECK(read((int)second, &byte, 1) == -1 && errno == EBADF);
    }
    else if (strcmp(mode, "closed") == 0)
    {
        errno = 0;
        CHECK(fcntl((int)first, F_GETFD) == -1 && errno == EBADF);
        CHECK(fcntl((int)second, F_GETFD) == 0);
    }
    // Stays alive until first reaches EOF
    else if (strcmp(mode, "hold") == 0)
    {
        char byte;
        CHECK(read((int)first, &byte, 1) == 0);
    }
    else if (strcmp(mode, "directory") == 0)
    {
        char here[64];
        CHECK(getcwd(here, sizeof(here)) != NULL && strcmp(here, "/usr/tests") == 0);
    }
    // A child of the old program is still this process's child
    else if (strcmp(mode, "reap") == 0)
    {
        int status = -1;
        CHECK(waitpid((pid_t)first, &status, 0) == first);
        CHECK(WIFEXITED(status) && WEXITSTATUS(status) == 9);
    }
    // first: execs left. second: the pid they all share
    else if (strcmp(mode, "chain") == 0)
    {
        CHECK(getpid() == second);
        if (first > 0)
        {
            exec_self("chain", first - 1, second);
            return EXEC_FAILED;
        }
    }
    else if (strcmp(mode, "environ") == 0)
    {
        CHECK(strcmp(getenv("EXEC_TEST"), "kept") == 0);
    }
    else if (strcmp(mode, "found") == 0)
    {
        CHECK(strcmp(argv[0], "exec") == 0);
    }
    else
    {
        CHECK(0);
    }

    return test_status();
}

static void test_fresh(void)
{
    pid_t parent = getpid();
    pid_t child = fork_flushed();
    CHECK(child >= 0);
    if (child == 0)
    {
        global = 99;
        zeroed = 99;
        exec_self("fresh", getpid(), parent);
        _exit(EXEC_FAILED);
    }
    child_passed(child);

    child = fork_flushed();
    CHECK(child >= 0);
    if (child == 0)
    {
        char *grown = sbrk(GROWTH); // the old end: the new one is GROWTH above it
        exec_self("break", (long)(uintptr_t)(grown + GROWTH), 0);
        _exit(EXEC_FAILED);
    }
    child_passed(child);
}

// Another program: the status comes back through the same wait
static void test_program(void)
{
    pid_t child = fork_flushed();
    CHECK(child >= 0);
    if (child == 0)
    {
        char *argv[] = {HELLO, NULL};
        execve(HELLO, argv, NULL);
        _exit(EXEC_FAILED);
    }

    int status = 0;
    CHECK(waitpid(child, &status, 0) == child);
    CHECK(WIFEXITED(status) && WEXITSTATUS(status) == 42);
}

static void test_failed_exec(void)
{
    char *argv[] = {self, "fresh", NULL};

    errno = 0;
    CHECK(execve("/no/such", argv, test_environ) == -1 && errno == ENOENT);
    errno = 0;
    CHECK(execve("/usr/tests", argv, test_environ) == -1 && errno == EACCES);
    errno = 0;
    CHECK(execve((const char *)1, argv, test_environ) == -1 && errno == EFAULT);
    errno = 0;
    CHECK(execve(self, (char *const *)1, test_environ) == -1 && errno == EFAULT);

    // Nothing happened to the caller: a close-on-exec fd is only closed by an exec that worked
    int fd = open(HELLO, O_RDONLY);
    CHECK(fd >= 0);
    CHECK(fcntl(fd, F_SETFD, FD_CLOEXEC) == 0);
    CHECK(execve("/no/such", argv, test_environ) == -1);
    CHECK(fcntl(fd, F_GETFD) == FD_CLOEXEC);
    close(fd);
    CHECK(global == 10);
}

// The flag belongs to the fd, not to the open file
static void test_flag(void)
{
    int fd = open(HELLO, O_RDONLY);
    CHECK(fd >= 0);
    CHECK(fcntl(fd, F_GETFD) == 0);
    CHECK(fcntl(fd, F_SETFD, FD_CLOEXEC) == 0);
    CHECK(fcntl(fd, F_GETFD) == FD_CLOEXEC);

    // dup and dup2 give a clear flag; dup2 onto itself changes nothing
    int copy = dup(fd);
    CHECK(copy >= 0 && fcntl(copy, F_GETFD) == 0);
    CHECK(fcntl(copy, F_SETFD, FD_CLOEXEC) == 0);
    CHECK(dup2(fd, copy) == copy);
    CHECK(fcntl(copy, F_GETFD) == 0);
    CHECK(dup2(fd, fd) == fd);
    CHECK(fcntl(fd, F_GETFD) == FD_CLOEXEC);

    // Clearing it, and a slot used again starts clear
    CHECK(fcntl(copy, F_SETFD, FD_CLOEXEC) == 0);
    CHECK(fcntl(copy, F_SETFD, 0) == 0);
    CHECK(fcntl(copy, F_GETFD) == 0);
    CHECK(fcntl(copy, F_SETFD, FD_CLOEXEC) == 0);
    CHECK(close(copy) == 0);
    CHECK(dup(fd) == copy);
    CHECK(fcntl(copy, F_GETFD) == 0);
    close(copy);

    // fork copies it. Not test_status(): a fork also copies the failures counted so far
    pid_t child = fork_flushed();
    CHECK(child >= 0);
    if (child == 0)
        _exit(fcntl(fd, F_GETFD) == FD_CLOEXEC ? 0 : 1);
    child_passed(child);
    close(fd);

    errno = 0;
    CHECK(fcntl(fd, F_GETFD) == -1 && errno == EBADF);
    errno = 0;
    CHECK(fcntl(99, F_GETFD) == -1 && errno == EBADF);
    errno = 0;
    CHECK(fcntl(0, 1000) == -1 && errno == EINVAL);
}

// Open fds stay, with their offsets; marked ones go
static void test_files(void)
{
    int kept = open(HELLO, O_RDONLY);
    int marked = open(HELLO, O_RDONLY);
    CHECK(kept >= 0 && marked >= 0);
    CHECK(fcntl(marked, F_SETFD, FD_CLOEXEC) == 0);

    pid_t child = fork_flushed();
    CHECK(child >= 0);
    if (child == 0)
    {
        char bytes[4];
        CHECK(read(kept, bytes, sizeof(bytes)) == 4);
        exec_self("files", kept, marked);
        _exit(EXEC_FAILED);
    }
    child_passed(child);
    close(kept);
    close(marked);

    int fds[2];
    CHECK(pipe(fds) == 0);
    child = fork_flushed();
    CHECK(child >= 0);
    if (child == 0)
    {
        close(fds[0]);
        exec_self("write", fds[1], 0);
        _exit(EXEC_FAILED);
    }

    close(fds[1]);
    char text[32];
    CHECK(read(fds[0], text, sizeof(text)) == 20);
    CHECK(memcmp(text, "from the new program", 20) == 0);
    CHECK(read(fds[0], text, sizeof(text)) == 0);
    close(fds[0]);
    child_passed(child);
}

// spawn is fork and exec in one: it honours the flag too
static void test_spawn(void)
{
    int marked = open(HELLO, O_RDONLY);
    int kept = open(HELLO, O_RDONLY);
    CHECK(marked >= 0 && kept >= 0);
    CHECK(fcntl(marked, F_SETFD, FD_CLOEXEC) == 0);

    char a[NUMBER_MAX], b[NUMBER_MAX];
    snprintf(a, sizeof(a), "%d", marked);
    snprintf(b, sizeof(b), "%d", kept);
    char *argv[] = {self, "closed", a, b, NULL};

    pid_t child = -1;
    CHECK(posix_spawn(&child, self, NULL, NULL, argv, test_environ) == 0);
    child_passed(child);
    close(marked);
    close(kept);
}

// The point of the flag: a live program holding no write end, so the reader sees EOF
static void test_eof(void)
{
    int data[2], stop[2];
    CHECK(pipe(data) == 0);
    CHECK(pipe(stop) == 0);
    CHECK(fcntl(data[1], F_SETFD, FD_CLOEXEC) == 0);

    pid_t child = fork_flushed();
    CHECK(child >= 0);
    if (child == 0)
    {
        close(stop[1]);
        exec_self("hold", stop[0], 0);
        _exit(EXEC_FAILED);
    }

    close(stop[0]);
    close(data[1]);
    char byte;
    CHECK(read(data[0], &byte, 1) == 0); // the child is alive, in its read
    close(data[0]);
    close(stop[1]);
    child_passed(child);
}

static void test_directory(void)
{
    pid_t child = fork_flushed();
    CHECK(child >= 0);
    if (child == 0)
    {
        CHECK(chdir("/usr/tests") == 0);
        exec_self("directory", 0, 0);
        _exit(EXEC_FAILED);
    }
    child_passed(child);
}

static void test_children(void)
{
    pid_t child = fork_flushed();
    CHECK(child >= 0);
    if (child == 0)
    {
        pid_t grandchild = fork_flushed();
        if (grandchild == 0)
            _exit(9);
        exec_self("reap", grandchild, 0);
        _exit(EXEC_FAILED);
    }
    child_passed(child);
}

// Each exec builds a whole address space and frees the old one, in one process
static void test_chain(void)
{
    pid_t child = fork_flushed();
    CHECK(child >= 0);
    if (child == 0)
    {
        exec_self("chain", CHAIN, getpid());
        _exit(EXEC_FAILED);
    }
    child_passed(child);
}

static void test_execv(void)
{
    pid_t child = fork_flushed();
    CHECK(child >= 0);
    if (child == 0)
    {
        static char *kept[] = {"EXEC_TEST=kept", NULL};
        environ = kept;
        char *argv[] = {self, "environ", NULL};
        execv(self, argv);
        _exit(EXEC_FAILED);
    }
    child_passed(child);
}

static void test_execvp(void)
{
    // Skips a directory without it, and argv[0] is what the caller passed
    pid_t child = fork_flushed();
    CHECK(child >= 0);
    if (child == 0)
    {
        static char *path[] = {"PATH=/nope:/usr/tests", NULL};
        environ = path;
        char *argv[] = {"exec", "found", NULL};
        execvp("exec", argv);
        _exit(EXEC_FAILED);
    }
    child_passed(child);

    // A name with '/' is used as given, whatever PATH says
    child = fork_flushed();
    CHECK(child >= 0);
    if (child == 0)
    {
        static char *path[] = {"PATH=/nope", NULL};
        environ = path;
        char *argv[] = {HELLO, NULL};
        execvp(HELLO, argv);
        _exit(EXEC_FAILED);
    }
    int status = 0;
    CHECK(waitpid(child, &status, 0) == child);
    CHECK(WIFEXITED(status) && WEXITSTATUS(status) == 42);

    char **original = environ;
    char *argv[] = {"nope", NULL};
    char *not_found[] = {"PATH=/nope:/bin", NULL};
    environ = not_found;
    errno = 0;
    CHECK(execvp("nope", argv) == -1 && errno == ENOENT);

    // Found but not runnable: EACCES wins over the entries without it
    char *denied[] = {"PATH=/usr:/nope", NULL};
    environ = denied;
    errno = 0;
    CHECK(execvp("tests", argv) == -1 && errno == EACCES);

    char *no_path[] = {"OTHER=1", NULL};
    environ = no_path;
    errno = 0;
    CHECK(execvp("hello", argv) == -1 && errno == ENOENT);
    environ = original;
}

int main(int argc, char **argv)
{
    self = argv[0];
    if (argc > 1)
        return child_main(argc, argv);

    test_fresh();
    test_program();
    test_failed_exec();
    test_flag();
    test_files();
    test_spawn();
    test_eof();
    test_directory();
    test_children();
    test_chain();
    test_execv();
    test_execvp();

    // Every pipe and file is closed again: the next free fd is 3
    int fd = dup(0);
    CHECK(fd == 3);
    close(fd);
    return test_status();
}
