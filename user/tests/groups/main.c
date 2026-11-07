#include <sys/wait.h>
#include <unistd.h>
#include <signal.h>
#include <spawn.h>
#include <errno.h>
#include <test.h>

/*
 * Process groups and sessions. A process is born in its parent's group and
 * session; setpgid moves it, setsid opens a new session; kill and wait then
 * address a whole group. This test itself sits in init's group and session, so
 * nothing here signals its own group: every target is a group made for the
 * case. Children count their own failures and report them through their exit
 * status.
 */

#define NO_SUCH_PID 999999
#define MEMBERS 3

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

// A child that only waits to be signalled
static pid_t child_sleeping(void)
{
    pid_t pid = fork_flushed();
    if (pid == 0)
        for (;;)
            pause();
    return pid;
}

// A child that blocks until the write end of fds closes, then exits with 0
static pid_t child_held(int fds[2])
{
    pid_t pid = fork_flushed();
    if (pid == 0)
    {
        char byte;
        close(fds[1]);
        read(fds[0], &byte, 1);
        _exit(0);
    }
    return pid;
}

// Blocks until nobody holds the write end any more: the end of whoever did
static void wait_for_close(int fds[2])
{
    char byte;
    close(fds[1]);
    CHECK(read(fds[0], &byte, 1) == 0);
    close(fds[0]);
}

static void test_inherited(void)
{
    pid_t group = getpgrp();
    pid_t session = getsid(0);
    CHECK(group > 0 && session > 0);
    CHECK(getpgid(0) == group && getpgid(getpid()) == group);
    CHECK(getsid(getpid()) == session);

    pid_t child = fork_flushed();
    CHECK(child >= 0);
    if (child == 0)
    {
        CHECK(getpgrp() == group);
        CHECK(getsid(0) == session);
        child_done();
    }

    // A parent can read a child's too, before it is waited for
    CHECK(getpgid(child) == group);
    CHECK(getsid(child) == session);
    child_passed(child);
}

static void test_setpgid(void)
{
    // By the child itself, group 0 meaning the one named after it
    pid_t child = fork_flushed();
    CHECK(child >= 0);
    if (child == 0)
    {
        CHECK(setpgid(0, 0) == 0);
        CHECK(getpgrp() == getpid());
        child_done();
    }
    child_passed(child);

    // By the parent, pid and group spelled out
    child = child_sleeping();
    CHECK(child >= 0);
    CHECK(setpgid(child, child) == 0);
    CHECK(getpgid(child) == child);
    CHECK(kill(child, SIGKILL) == 0);
    child_killed_by(child, SIGKILL);

    // Joining a group that exists: a pipeline's later stages, and it outlives its first member
    pid_t first = child_sleeping();
    pid_t second = child_sleeping();
    CHECK(first >= 0 && second >= 0);
    CHECK(setpgid(first, first) == 0);
    CHECK(setpgid(second, first) == 0);
    CHECK(getpgid(second) == first);
    CHECK(kill(first, SIGKILL) == 0);
    child_killed_by(first, SIGKILL);
    CHECK(getpgid(second) == first);
    CHECK(kill(-first, 0) == 0);
    CHECK(kill(second, SIGKILL) == 0);
    child_killed_by(second, SIGKILL);
}

static void test_setpgid_errors(void)
{
    errno = 0;
    CHECK(setpgid(-1, 0) == -1 && errno == EINVAL);
    errno = 0;
    CHECK(setpgid(0, -1) == -1 && errno == EINVAL);

    // Neither itself nor its child
    errno = 0;
    CHECK(setpgid(NO_SUCH_PID, 0) == -1 && errno == ESRCH);
    errno = 0;
    CHECK(setpgid(1, 1) == -1 && errno == ESRCH);

    // A group that does not exist can only be the process's own
    errno = 0;
    CHECK(setpgid(0, NO_SUCH_PID) == -1 && errno == EPERM);

    errno = 0;
    CHECK(getpgid(-1) == -1 && errno == ESRCH);
    errno = 0;
    CHECK(getpgid(NO_SUCH_PID) == -1 && errno == ESRCH);
    errno = 0;
    CHECK(getsid(-1) == -1 && errno == ESRCH);
    errno = 0;
    CHECK(getsid(NO_SUCH_PID) == -1 && errno == ESRCH);
}

// Once a child runs its program, only the program may change its group
static void test_after_exec(void)
{
    char *argv[] = {"/usr/tests/hello", NULL};
    char *envp[] = {NULL};

    // The group chosen before exec stays. Its exit closes the pipe: still there until waited for
    int fds[2];
    CHECK(pipe(fds) == 0);
    pid_t child = fork_flushed();
    CHECK(child >= 0);
    if (child == 0)
    {
        close(fds[0]);
        setpgid(0, 0);
        execve(argv[0], argv, envp);
        _exit(127);
    }
    wait_for_close(fds);
    CHECK(getpgid(child) == child);
    errno = 0;
    CHECK(setpgid(child, 0) == -1 && errno == EACCES);
    CHECK(waitpid(child, NULL, 0) == child);

    // spawn is a fork and an exec in one
    pid_t spawned;
    CHECK(posix_spawn(&spawned, argv[0], NULL, NULL, argv, envp) == 0);
    errno = 0;
    CHECK(setpgid(spawned, spawned) == -1 && errno == EACCES);
    CHECK(waitpid(spawned, NULL, 0) == spawned);
}

static void test_session(void)
{
    pid_t session = getsid(0);
    pid_t group = getpgrp();

    int fds[2];
    CHECK(pipe(fds) == 0);

    pid_t child = fork_flushed();
    CHECK(child >= 0);
    if (child == 0)
    {
        close(fds[0]);
        pid_t self = getpid();
        CHECK(setsid() == self);
        CHECK(getsid(0) == self);
        CHECK(getpgrp() == self);

        // A leader stays where it is
        errno = 0;
        CHECK(setsid() == -1 && errno == EPERM);
        errno = 0;
        CHECK(setpgid(0, 0) == -1 && errno == EPERM);

        // Its child starts in the new session, and cannot reach the old one's groups
        pid_t grandchild = fork_flushed();
        CHECK(grandchild >= 0);
        if (grandchild == 0)
        {
            CHECK(getsid(0) == self);
            errno = 0;
            CHECK(setpgid(0, group) == -1 && errno == EPERM);
            child_done();
        }
        child_passed(grandchild);
        close(fds[1]);
        child_done();
    }

    // Past its setsid: its pipe end closes after it. Other session: not movable, even as a child
    wait_for_close(fds);
    CHECK(getsid(child) == child);
    CHECK(getsid(0) == session);
    errno = 0;
    CHECK(setpgid(child, 0) == -1 && errno == EPERM);
    child_passed(child);

    // A group's first member cannot leave: its group would stay behind with its name
    child = fork_flushed();
    CHECK(child >= 0);
    if (child == 0)
    {
        CHECK(setpgid(0, 0) == 0);
        errno = 0;
        CHECK(setsid() == -1 && errno == EPERM);
        child_done();
    }
    child_passed(child);
}

// Every member, nobody else
static void test_kill_group(void)
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

    CHECK(kill(-members[0], 0) == 0);
    CHECK(kill(-members[0], SIGTERM) == 0);
    for (int i = 0; i < MEMBERS; i++)
        child_killed_by(members[i], SIGTERM);

    errno = 0;
    CHECK(kill(-members[0], SIGTERM) == -1 && errno == ESRCH);

    CHECK(kill(outsider, 0) == 0);
    CHECK(kill(outsider, SIGKILL) == 0);
    child_killed_by(outsider, SIGKILL);
}

// kill(0): the sender's own group, the sender included. The pipe closes when the last one is gone
static void test_kill_own_group(void)
{
    int fds[2];
    CHECK(pipe(fds) == 0);

    pid_t child = fork_flushed();
    CHECK(child >= 0);
    if (child == 0)
    {
        close(fds[0]);
        setpgid(0, 0);
        for (int i = 0; i < MEMBERS; i++)
            if (fork_flushed() == 0)
                for (;;)
                    pause();

        kill(0, SIGTERM);
        for (;;)
            pause(); // not reached
    }

    wait_for_close(fds);
    child_killed_by(child, SIGTERM);
}

// An exited member still counts until it is waited for
static void test_exited_member(void)
{
    int fds[2];
    CHECK(pipe(fds) == 0);

    pid_t child = fork_flushed();
    CHECK(child >= 0);
    if (child == 0)
    {
        close(fds[0]);
        _exit(0);
    }

    CHECK(setpgid(child, child) == 0);
    wait_for_close(fds);
    CHECK(kill(-child, 0) == 0);
    CHECK(waitpid(child, NULL, 0) == child);
    errno = 0;
    CHECK(kill(-child, 0) == -1 && errno == ESRCH);
}

static void test_wait_group(void)
{
    int fds[2];
    CHECK(pipe(fds) == 0);

    pid_t first = child_held(fds);
    pid_t second = child_held(fds);
    pid_t outsider = child_held(fds);
    pid_t same_group = child_held(fds);
    CHECK(first >= 0 && second >= 0 && outsider >= 0 && same_group >= 0);

    CHECK(setpgid(first, first) == 0);
    CHECK(setpgid(second, first) == 0);
    CHECK(setpgid(outsider, outsider) == 0);

    wait_for_close(fds);

    // By group: its two members, in either order, and the others stay
    pid_t a = waitpid(-first, NULL, 0);
    pid_t b = waitpid(-first, NULL, 0);
    CHECK((a == first && b == second) || (a == second && b == first));
    errno = 0;
    CHECK(waitpid(-first, NULL, 0) == -1 && errno == ECHILD);

    // 0: only the child still in our group
    CHECK(waitpid(0, NULL, 0) == same_group);
    errno = 0;
    CHECK(waitpid(0, NULL, 0) == -1 && errno == ECHILD);

    CHECK(waitpid(outsider, NULL, 0) == outsider);
}

int main(void)
{
    test_inherited();
    test_setpgid();
    test_setpgid_errors();
    test_after_exec();
    test_session();
    test_kill_group();
    test_kill_own_group();
    test_exited_member();
    test_wait_group();

    // Every pipe is closed again: the next free fd is 3
    int fd = dup(0);
    CHECK(fd == 3);
    close(fd);
    return test_status();
}
