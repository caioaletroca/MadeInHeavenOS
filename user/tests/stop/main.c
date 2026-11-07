#include <sys/wait.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <errno.h>
#include <test.h>

/*
 * Stopping and resuming. Four signals stop a process by default; SIGCONT
 * resumes it, whatever its own action is; SIGKILL ends it anyway; everything
 * else waits for it to run again. waitpid reports a stop once, with WUNTRACED,
 * which also makes the tests' waits exact: when it returns, the child stopped.
 * Children count their own failures and report them through their exit status.
 */

#define MEMBERS 3

static const int stop_signals[] = {SIGSTOP, SIGTSTP, SIGTTIN, SIGTTOU};

static volatile sig_atomic_t handled;

static void on_signal(int signal)
{
    handled = signal;
}

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

static void child_stopped_by(pid_t pid, int signal)
{
    int status = 0;
    CHECK(waitpid(pid, &status, WUNTRACED) == pid);
    CHECK(WIFSTOPPED(status) && WSTOPSIG(status) == signal);
    CHECK(!WIFEXITED(status) && !WIFSIGNALED(status));
}

static pid_t child_sleeping(void)
{
    pid_t pid = fork_flushed();
    if (pid == 0)
        for (;;)
            pause();
    return pid;
}

// No system call on its side: only the timer brings it back through the kernel
static pid_t child_running(void)
{
    pid_t pid = fork_flushed();
    if (pid == 0)
        for (;;)
            ;
    return pid;
}

static void test_each_stops(void)
{
    for (size_t i = 0; i < sizeof(stop_signals) / sizeof(stop_signals[0]); i++)
    {
        pid_t child = child_sleeping();
        CHECK(child >= 0);
        CHECK(kill(child, stop_signals[i]) == 0);
        child_stopped_by(child, stop_signals[i]);
        CHECK(kill(child, SIGKILL) == 0);
        child_killed_by(child, SIGKILL);
    }
}

// A process in a loop stops too, and resumes, as many times as it is asked
static void test_running(void)
{
    pid_t child = child_running();
    CHECK(child >= 0);
    for (int i = 0; i < MEMBERS; i++)
    {
        CHECK(kill(child, SIGSTOP) == 0);
        child_stopped_by(child, SIGSTOP);
        CHECK(kill(child, SIGCONT) == 0);
    }
    CHECK(kill(child, SIGKILL) == 0);
    child_killed_by(child, SIGKILL);
}

// A stop is reported once, and only to whoever asked for it
static void test_reported_once(void)
{
    pid_t child = child_sleeping();
    CHECK(child >= 0);
    CHECK(kill(child, SIGSTOP) == 0);
    child_stopped_by(child, SIGSTOP);

    CHECK(waitpid(child, NULL, WUNTRACED | WNOHANG) == 0);
    CHECK(waitpid(child, NULL, WNOHANG) == 0);

    // A new stop is a new report
    CHECK(kill(child, SIGCONT) == 0);
    CHECK(kill(child, SIGTSTP) == 0);
    child_stopped_by(child, SIGTSTP);

    CHECK(kill(child, SIGKILL) == 0);
    child_killed_by(child, SIGKILL);
}

static void test_wnohang(void)
{
    errno = 0;
    CHECK(waitpid(-1, NULL, WNOHANG) == -1 && errno == ECHILD);
    errno = 0;
    CHECK(waitpid(-1, NULL, 0x100) == -1 && errno == EINVAL);

    pid_t child = child_sleeping();
    CHECK(child >= 0);
    CHECK(waitpid(child, NULL, WNOHANG) == 0);
    CHECK(kill(child, SIGKILL) == 0);
    child_killed_by(child, SIGKILL);
}

// The system call in progress goes on as if nothing happened: no EINTR for the program
static void test_read_continues(void)
{
    int fds[2];
    CHECK(pipe(fds) == 0);

    pid_t child = fork_flushed();
    CHECK(child >= 0);
    if (child == 0)
    {
        char byte = 0;
        close(fds[1]);
        CHECK(read(fds[0], &byte, 1) == 1 && byte == 'x');
        child_done();
    }

    close(fds[0]);
    CHECK(kill(child, SIGTSTP) == 0);
    child_stopped_by(child, SIGTSTP);

    // Stopped, it does not take the byte
    CHECK(write(fds[1], "x", 1) == 1);
    CHECK(waitpid(child, NULL, WNOHANG) == 0);

    CHECK(kill(child, SIGCONT) == 0);
    child_passed(child);
    close(fds[1]);
}

// Anything but SIGKILL and SIGCONT waits for the process to run again
static void test_signal_waits(void)
{
    pid_t child = child_sleeping();
    CHECK(child >= 0);
    CHECK(kill(child, SIGSTOP) == 0);
    child_stopped_by(child, SIGSTOP);

    CHECK(kill(child, SIGTERM) == 0);
    CHECK(waitpid(child, NULL, WNOHANG) == 0);

    CHECK(kill(child, SIGCONT) == 0);
    child_killed_by(child, SIGTERM);
}

// SIGCONT resumes before its action is even looked at, and a handler then runs
static void test_continue_actions(void)
{
    pid_t child = fork_flushed();
    CHECK(child >= 0);
    if (child == 0)
    {
        signal(SIGCONT, SIG_IGN);
        raise(SIGSTOP);
        child_done();
    }
    child_stopped_by(child, SIGSTOP);
    CHECK(kill(child, SIGCONT) == 0);
    child_passed(child);

    child = fork_flushed();
    CHECK(child >= 0);
    if (child == 0)
    {
        sigset_t block;
        sigemptyset(&block);
        sigaddset(&block, SIGCONT);
        sigprocmask(SIG_BLOCK, &block, NULL);
        raise(SIGSTOP);
        child_done();
    }
    child_stopped_by(child, SIGSTOP);
    CHECK(kill(child, SIGCONT) == 0);
    child_passed(child);

    child = fork_flushed();
    CHECK(child >= 0);
    if (child == 0)
    {
        signal(SIGCONT, on_signal);
        raise(SIGSTOP);
        CHECK(handled == SIGCONT);
        child_done();
    }
    child_stopped_by(child, SIGSTOP);
    CHECK(kill(child, SIGCONT) == 0);
    child_passed(child);
}

// A stop signal that has not acted yet is cancelled by SIGCONT
static void test_continue_cancels_stop(void)
{
    int go[2], ready[2];
    CHECK(pipe(go) == 0);
    CHECK(pipe(ready) == 0);

    pid_t child = fork_flushed();
    CHECK(child >= 0);
    if (child == 0)
    {
        char byte;
        sigset_t block;
        sigemptyset(&block);
        sigaddset(&block, SIGTSTP);
        sigprocmask(SIG_BLOCK, &block, NULL);

        close(go[1]);
        close(ready[0]);
        close(ready[1]); // blocked: the test may send

        CHECK(read(go[0], &byte, 1) == 0);

        sigset_t waiting;
        CHECK(sigpending(&waiting) == 0);
        CHECK(sigismember(&waiting, SIGTSTP) == 0);
        sigprocmask(SIG_UNBLOCK, &block, NULL);
        child_done();
    }

    char byte;
    close(ready[1]);
    CHECK(read(ready[0], &byte, 1) == 0);
    close(ready[0]);

    CHECK(kill(child, SIGTSTP) == 0);
    CHECK(kill(child, SIGCONT) == 0);
    close(go[0]);
    close(go[1]);

    int status = -1;
    CHECK(waitpid(child, &status, WUNTRACED) == child);
    CHECK(WIFEXITED(status) && WEXITSTATUS(status) == 0);
}

// A program can decline: a handler or SIG_IGN, except for SIGSTOP
static void test_declined(void)
{
    pid_t child = fork_flushed();
    CHECK(child >= 0);
    if (child == 0)
    {
        signal(SIGTSTP, on_signal);
        raise(SIGTSTP);
        CHECK(handled == SIGTSTP);

        signal(SIGTSTP, SIG_IGN);
        raise(SIGTSTP);
        child_done();
    }

    int status = -1;
    CHECK(waitpid(child, &status, WUNTRACED) == child);
    CHECK(WIFEXITED(status) && WEXITSTATUS(status) == 0);
}

// The parent hears of it through SIGCHLD too, and asleep in wait it is woken
static void test_parent_told(void)
{
    signal(SIGCHLD, on_signal);
    handled = 0;

    pid_t child = child_sleeping();
    CHECK(child >= 0);
    CHECK(kill(child, SIGSTOP) == 0);
    child_stopped_by(child, SIGSTOP);
    CHECK(handled == SIGCHLD);

    signal(SIGCHLD, SIG_DFL);
    CHECK(kill(child, SIGKILL) == 0);
    child_killed_by(child, SIGKILL);
}

static void test_group(void)
{
    pid_t members[MEMBERS];
    for (int i = 0; i < MEMBERS; i++)
    {
        members[i] = child_sleeping();
        CHECK(members[i] >= 0);
        CHECK(setpgid(members[i], members[0]) == 0);
    }

    CHECK(kill(-members[0], SIGSTOP) == 0);
    for (int i = 0; i < MEMBERS; i++)
        child_stopped_by(members[i], SIGSTOP);

    CHECK(kill(-members[0], SIGCONT) == 0);
    CHECK(kill(-members[0], SIGTERM) == 0);
    for (int i = 0; i < MEMBERS; i++)
        child_killed_by(members[i], SIGTERM);
}

// init takes no signal it has no handler for: a stop would end this run
static void test_init(void)
{
    CHECK(kill(1, SIGSTOP) == 0);
    CHECK(kill(1, SIGCONT) == 0);
    CHECK(kill(1, 0) == 0);
}

int main(void)
{
    test_each_stops();
    test_running();
    test_reported_once();
    test_wnohang();
    test_read_continues();
    test_signal_waits();
    test_continue_actions();
    test_continue_cancels_stop();
    test_declined();
    test_parent_told();
    test_group();
    test_init();

    // Every pipe is closed again: the next free fd is 3
    int fd = dup(0);
    CHECK(fd == 3);
    close(fd);
    return test_status();
}
