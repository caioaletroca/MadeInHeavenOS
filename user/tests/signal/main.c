#include <sys/wait.h>
#include <unistd.h>
#include <signal.h>
#include <stdlib.h>
#include <errno.h>
#include <test.h>

/*
 * kill and the signals' default actions. A signal is only recorded when it is
 * sent; the target acts on it on its way back to user mode, wherever that is:
 * a program in a loop was stopped by a timer tick, so it acts when it is next
 * resumed, and one asleep in the kernel has to be woken first. Most signals end
 * the process, a few do nothing. Children count their own failures and report
 * them through their exit status.
 */

#define SPINS 1000000 // time for a child to reach its sleep
#define ROUNDS 200
#define NO_SUCH_PID 999999
#define PIPE_OVERFILL 8192 // more than a pipe holds

static const int fatal_signals[] = {
    SIGHUP, SIGINT, SIGQUIT, SIGILL, SIGTRAP, SIGABRT, SIGBUS, SIGFPE, SIGKILL,
    SIGUSR1, SIGSEGV, SIGUSR2, SIGPIPE, SIGALRM, SIGTERM,
};

static const int ignored_signals[] = {SIGCHLD, SIGCONT, SIGURG, SIGWINCH};

static pid_t fork_flushed(void)
{
    fflush(stdout);
    pid_t pid = fork();
    if (pid == 0)
        test_failures = 0; // a child reports only its own
    return pid;
}

// Leaves the child: exit status = its failed checks. _exit, so atexit never runs twice
static void child_done(void)
{
    fflush(stdout);
    _exit(test_status());
}

static void spin(void)
{
    for (volatile int i = 0; i < SPINS; i++)
        ;
}

static void child_killed_by(pid_t pid, int signal)
{
    int status = 0;
    CHECK(waitpid(pid, &status, 0) == pid);
    CHECK(WIFSIGNALED(status) && WTERMSIG(status) == signal);
}

static void child_passed(pid_t pid)
{
    int status = -1;
    CHECK(waitpid(pid, &status, 0) == pid);
    CHECK(WIFEXITED(status) && WEXITSTATUS(status) == 0);
}

// Each of them ends the process that sends it to itself, on the way out of kill
static void test_fatal_to_self(void)
{
    for (size_t i = 0; i < sizeof(fatal_signals) / sizeof(fatal_signals[0]); i++)
    {
        pid_t child = fork_flushed();
        CHECK(child >= 0);
        if (child == 0)
        {
            kill(getpid(), fatal_signals[i]);
            _exit(0); // not reached
        }
        child_killed_by(child, fatal_signals[i]);
    }
}

static void test_raise_and_abort(void)
{
    pid_t child = fork_flushed();
    CHECK(child >= 0);
    if (child == 0)
    {
        raise(SIGTERM);
        _exit(0); // not reached
    }
    child_killed_by(child, SIGTERM);

    child = fork_flushed();
    CHECK(child >= 0);
    if (child == 0)
        abort();
    child_killed_by(child, SIGABRT);
}

// Nothing happens, and execution goes on after kill
static void test_ignored_by_default(void)
{
    for (size_t i = 0; i < sizeof(ignored_signals) / sizeof(ignored_signals[0]); i++)
    {
        pid_t child = fork_flushed();
        CHECK(child >= 0);
        if (child == 0)
        {
            CHECK(kill(getpid(), ignored_signals[i]) == 0);
            child_done();
        }
        child_passed(child);
    }
}

// No system call on its side: only the timer brings it back through the kernel
static void test_running(void)
{
    pid_t child = fork_flushed();
    CHECK(child >= 0);
    if (child == 0)
        for (;;)
            ;

    spin();
    CHECK(kill(child, SIGTERM) == 0);
    child_killed_by(child, SIGTERM);
}

// Asleep in read: the signal has to wake it
static void test_sleeping_in_read(void)
{
    int fds[2];
    CHECK(pipe(fds) == 0);

    pid_t child = fork_flushed();
    CHECK(child >= 0);
    if (child == 0)
    {
        char byte;
        read(fds[0], &byte, 1); // never gets data: this process holds the write end too
        _exit(0);               // not reached
    }

    spin();
    CHECK(kill(child, SIGTERM) == 0);
    child_killed_by(child, SIGTERM);
    close(fds[0]);
    close(fds[1]);
}

// Asleep in write, on a full pipe
static void test_sleeping_in_write(void)
{
    static const char data[PIPE_OVERFILL] = {0};
    int fds[2];
    CHECK(pipe(fds) == 0);

    pid_t child = fork_flushed();
    CHECK(child >= 0);
    if (child == 0)
    {
        write(fds[1], data, sizeof(data));
        _exit(0); // not reached
    }

    spin();
    CHECK(kill(child, SIGKILL) == 0);
    child_killed_by(child, SIGKILL);
    close(fds[0]);
    close(fds[1]);
}

// Asleep in wait, for a child that will not leave on its own
static void test_sleeping_in_wait(void)
{
    int fds[2];
    CHECK(pipe(fds) == 0);

    pid_t child = fork_flushed();
    CHECK(child >= 0);
    if (child == 0)
    {
        pid_t grandchild = fork_flushed();
        if (grandchild == 0)
        {
            char byte;
            close(fds[1]);
            read(fds[0], &byte, 1); // EOF once the test closes its end
            _exit(0);
        }
        waitpid(grandchild, NULL, 0);
        _exit(0); // not reached
    }

    spin();
    CHECK(kill(child, SIGTERM) == 0);
    child_killed_by(child, SIGTERM);
    close(fds[0]);
    close(fds[1]);
}

// Asleep in read on stdin too. Whatever stdin is, the loop keeps it there until the signal
static void test_sleeping_on_stdin(void)
{
    pid_t child = fork_flushed();
    CHECK(child >= 0);
    if (child == 0)
    {
        char byte;
        for (;;)
            read(0, &byte, 1);
    }

    spin();
    CHECK(kill(child, SIGTERM) == 0);
    child_killed_by(child, SIGTERM);
}

// A signal that does nothing must not disturb a sleep: read goes on to get the byte
static void test_ignored_keeps_sleeping(void)
{
    int fds[2];
    CHECK(pipe(fds) == 0);

    pid_t child = fork_flushed();
    CHECK(child >= 0);
    if (child == 0)
    {
        char byte = 0;
        CHECK(read(fds[0], &byte, 1) == 1 && byte == 'x');
        child_done();
    }

    spin();
    CHECK(kill(child, SIGCHLD) == 0);
    spin();
    CHECK(write(fds[1], "x", 1) == 1);
    child_passed(child);
    close(fds[0]);
    close(fds[1]);
}

// Dying is an exit: its fds close, so a reader of its pipe gets EOF
static void test_files_closed(void)
{
    int data[2], hold[2];
    CHECK(pipe(data) == 0);
    CHECK(pipe(hold) == 0);

    pid_t child = fork_flushed();
    CHECK(child >= 0);
    if (child == 0)
    {
        close(data[0]);
        close(hold[1]);
        char byte;
        read(hold[0], &byte, 1);
        _exit(0); // not reached
    }

    close(data[1]);
    spin();
    CHECK(kill(child, SIGTERM) == 0);

    char byte;
    CHECK(read(data[0], &byte, 1) == 0);
    child_killed_by(child, SIGTERM);
    close(data[0]);
    close(hold[0]);
    close(hold[1]);
}

// Between processes: a child can signal its parent
static void test_child_kills_parent(void)
{
    pid_t child = fork_flushed();
    CHECK(child >= 0);
    if (child == 0)
    {
        pid_t grandchild = fork_flushed();
        if (grandchild == 0)
        {
            kill(getppid(), SIGUSR1);
            _exit(0);
        }
        waitpid(grandchild, NULL, 0);
        _exit(0); // not reached
    }
    child_killed_by(child, SIGUSR1);
}

static void test_errors(void)
{
    // Signal 0 checks that a process exists and sends nothing
    CHECK(kill(getpid(), 0) == 0);
    errno = 0;
    CHECK(kill(NO_SUCH_PID, 0) == -1 && errno == ESRCH);
    errno = 0;
    CHECK(kill(NO_SUCH_PID, SIGTERM) == -1 && errno == ESRCH);

    errno = 0;
    CHECK(kill(getpid(), -1) == -1 && errno == EINVAL);
    errno = 0;
    CHECK(kill(getpid(), NSIG) == -1 && errno == EINVAL);

    // Process groups and "everyone" do not exist yet (C6)
    errno = 0;
    CHECK(kill(0, SIGTERM) == -1 && errno == ESRCH);
    errno = 0;
    CHECK(kill(-1, SIGTERM) == -1 && errno == ESRCH);
    errno = 0;
    CHECK(kill(-5, SIGTERM) == -1 && errno == ESRCH);
}

// An exited child still exists until it is waited for, and it is too late to change how it ended
static void test_exited_child(void)
{
    int fds[2];
    CHECK(pipe(fds) == 0);

    pid_t child = fork_flushed();
    CHECK(child >= 0);
    if (child == 0)
    {
        close(fds[0]);
        _exit(5); // closing the write end tells the parent
    }

    close(fds[1]);
    char byte;
    CHECK(read(fds[0], &byte, 1) == 0);
    close(fds[0]);

    CHECK(kill(child, 0) == 0);
    CHECK(kill(child, SIGKILL) == 0);

    int status = 0;
    CHECK(waitpid(child, &status, 0) == child);
    CHECK(WIFEXITED(status) && WEXITSTATUS(status) == 5);

    errno = 0;
    CHECK(kill(child, 0) == -1 && errno == ESRCH);
}

// init takes no signal it has no handler for: this run would end with it
static void test_init(void)
{
    CHECK(kill(1, 0) == 0);
    CHECK(kill(1, SIGTERM) == 0);
    CHECK(kill(1, SIGKILL) == 0);
    spin();
    CHECK(kill(1, 0) == 0);
}

// Every killed process gives everything back
static void test_rounds(void)
{
    for (int i = 0; i < ROUNDS; i++)
    {
        pid_t child = fork_flushed();
        CHECK(child >= 0);
        if (child == 0)
            for (;;)
                ;

        CHECK(kill(child, SIGKILL) == 0);
        child_killed_by(child, SIGKILL);
    }
}

int main(void)
{
    test_fatal_to_self();
    test_raise_and_abort();
    test_ignored_by_default();
    test_running();
    test_sleeping_in_read();
    test_sleeping_in_write();
    test_sleeping_in_wait();
    test_sleeping_on_stdin();
    test_ignored_keeps_sleeping();
    test_files_closed();
    test_child_kills_parent();
    test_errors();
    test_exited_child();
    test_init();
    test_rounds();

    // Every pipe is closed again: the next free fd is 3
    int fd = dup(0);
    CHECK(fd == 3);
    close(fd);
    return test_status();
}
