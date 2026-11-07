#include <sys/ioctl.h>
#include <sys/wait.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <errno.h>
#include <test.h>

/*
 * The terminal and its foreground group. init opens a session for the runner
 * and gives it the console, so this test is in the session that owns it. No
 * keyboard here: TIOCSTI pushes a byte into the console's input, the same path
 * a key takes. The foreground group is always handed back to this test's group
 * before a case ends: the runner reads nothing, but the next case relies on it.
 */

#define SPINS 1000000 // time for a child to reach its sleep
#define NO_SUCH_PID 999999
#define MEMBERS 3
#define CTRL_C 0x03
#define CTRL_BACKSLASH 0x1C

static volatile sig_atomic_t interrupted;

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

static pid_t child_sleeping(void)
{
    pid_t pid = fork_flushed();
    if (pid == 0)
        for (;;)
            pause();
    return pid;
}

static void press(const char *keys)
{
    for (; *keys != '\0'; keys++)
        CHECK(ioctl(0, TIOCSTI, keys) == 0);
}

static void press_byte(char byte)
{
    CHECK(ioctl(0, TIOCSTI, &byte) == 0);
}

static void take_foreground_back(void)
{
    CHECK(tcsetpgrp(0, getpgrp()) == 0);
    CHECK(tcgetpgrp(0) == getpgrp());
}

// Taking the terminal needs a session leader, and a terminal nobody else's leader holds
static void test_take(void)
{
    errno = 0;
    CHECK(ioctl(0, TIOCSCTTY, NULL) == -1 && errno == EPERM);

    pid_t child = fork_flushed();
    CHECK(child >= 0);
    if (child == 0)
    {
        CHECK(setsid() == getpid());
        errno = 0;
        CHECK(ioctl(0, TIOCSCTTY, NULL) == -1 && errno == EPERM);
        child_done();
    }
    child_passed(child);
}

static void test_foreground_errors(void)
{
    take_foreground_back();

    errno = 0;
    CHECK(tcsetpgrp(0, NO_SUCH_PID) == -1 && errno == EPERM);
    errno = 0;
    CHECK(tcsetpgrp(0, -1) == -1 && errno == EINVAL);
    errno = 0;
    CHECK(tcsetpgrp(0, 0) == -1 && errno == EINVAL);

    // A pipe is no terminal
    int fds[2];
    CHECK(pipe(fds) == 0);
    errno = 0;
    CHECK(tcgetpgrp(fds[0]) == -1 && errno == ENOTTY);
    errno = 0;
    CHECK(tcsetpgrp(fds[0], getpgrp()) == -1 && errno == ENOTTY);
    close(fds[0]);
    close(fds[1]);

    // Another session: not its terminal, whatever fd it holds
    pid_t child = fork_flushed();
    CHECK(child >= 0);
    if (child == 0)
    {
        char byte = 'x';
        setsid();
        errno = 0;
        CHECK(tcgetpgrp(0) == -1 && errno == ENOTTY);
        errno = 0;
        CHECK(tcsetpgrp(0, getpgrp()) == -1 && errno == ENOTTY);
        errno = 0;
        CHECK(ioctl(0, TIOCSTI, &byte) == -1 && errno == EPERM);
        child_done();
    }
    child_passed(child);
}

// A key reaches every member of the foreground group, nobody else, and this test goes on
static void test_key_signals(char key, int signal)
{
    pid_t members[MEMBERS];
    for (int i = 0; i < MEMBERS; i++)
    {
        members[i] = child_sleeping();
        CHECK(members[i] >= 0);
        CHECK(setpgid(members[i], members[0]) == 0);
    }

    pid_t outsider = child_sleeping();
    CHECK(outsider >= 0);
    CHECK(setpgid(outsider, outsider) == 0);

    CHECK(tcsetpgrp(0, members[0]) == 0);
    CHECK(tcgetpgrp(0) == members[0]);
    press_byte(key);
    for (int i = 0; i < MEMBERS; i++)
        child_killed_by(members[i], signal);
    take_foreground_back();

    CHECK(kill(outsider, 0) == 0);
    CHECK(kill(outsider, SIGKILL) == 0);
    child_killed_by(outsider, SIGKILL);
}

// The foreground group is gone: the key has nobody to reach, and nothing breaks
static void test_no_foreground_left(void)
{
    pid_t child = child_sleeping();
    CHECK(child >= 0);
    CHECK(setpgid(child, child) == 0);
    CHECK(tcsetpgrp(0, child) == 0);
    CHECK(kill(child, SIGKILL) == 0);
    child_killed_by(child, SIGKILL);

    press_byte(CTRL_C);
    take_foreground_back();
}

static void on_interrupt(int signal)
{
    (void)signal;
    interrupted = 1;
}

// A program that catches it: read ends with EINTR, the way a shell's prompt comes back
static void test_handler_ends_read(void)
{
    int ready[2];
    CHECK(pipe(ready) == 0);

    pid_t child = fork_flushed();
    CHECK(child >= 0);
    if (child == 0)
    {
        struct sigaction action;
        memset(&action, 0, sizeof(action));
        action.sa_handler = on_interrupt;
        CHECK(sigaction(SIGINT, &action, NULL) == 0);
        CHECK(setpgid(0, 0) == 0);
        close(ready[0]);
        close(ready[1]); // tells the test: group joined, handler in place

        char byte;
        errno = 0;
        CHECK(read(0, &byte, 1) == -1 && errno == EINTR);
        CHECK(interrupted);
        child_done();
    }

    char byte;
    close(ready[1]);
    CHECK(read(ready[0], &byte, 1) == 0);
    close(ready[0]);

    CHECK(tcsetpgrp(0, child) == 0);
    spin();
    press_byte(CTRL_C);
    child_passed(child);
    take_foreground_back();
}

// The keys drop what is waiting: the line being typed and lines nobody has read
static void test_input_dropped(void)
{
    pid_t child = child_sleeping();
    CHECK(child >= 0);
    CHECK(setpgid(child, child) == 0);
    CHECK(tcsetpgrp(0, child) == 0);

    press("read by nobody\n");
    press("half a line");
    press_byte(CTRL_C);
    child_killed_by(child, SIGINT);
    take_foreground_back();

    press("kept\n");
    char line[16];
    CHECK(read(0, line, sizeof(line)) == 5);
    CHECK(memcmp(line, "kept\n", 5) == 0);
}

int main(void)
{
    test_take();
    test_foreground_errors();
    test_key_signals(CTRL_C, SIGINT);
    test_key_signals(CTRL_BACKSLASH, SIGQUIT);
    test_no_foreground_left();
    test_handler_ends_read();
    test_input_dropped();

    // Every pipe is closed again: the next free fd is 3
    int fd = dup(0);
    CHECK(fd == 3);
    close(fd);
    return test_status();
}
