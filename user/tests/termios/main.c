#include <sys/ioctl.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <errno.h>
#include <test.h>

/*
 * The terminal's settings. Like the terminal test, this has no keyboard:
 * TIOCSTI pushes a byte into the console's input. Enter sends '\r' and
 * Backspace 0x7F, as a terminal does, so those are the bytes pressed here. The
 * keys with no character, like the arrows, have no byte to push: the kernel's
 * console self-test covers them.
 *
 * Every case ends by putting the settings back and dropping what is waiting:
 * the next case, and the next test, rely on it.
 */

#define SPINS 1000000 // time for a child to reach its sleep
#define WHEN_NOT_VALID 99
#define CTRL_C 0x03
#define CTRL_D 0x04
#define CTRL_U 0x15
#define CTRL_Z 0x1A
#define CTRL_BACKSLASH 0x1C
#define ESCAPE 0x1B
#define BACKSPACE 0x7F

static struct termios original;

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

static void child_passed(pid_t pid)
{
    int status = -1;
    CHECK(waitpid(pid, &status, 0) == pid);
    CHECK(WIFEXITED(status) && WEXITSTATUS(status) == 0);
}

static void child_killed_by(pid_t pid, int signal)
{
    int status = 0;
    CHECK(waitpid(pid, &status, 0) == pid);
    CHECK(WIFSIGNALED(status) && WTERMSIG(status) == signal);
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

static struct termios settings_raw(void)
{
    struct termios settings = original;
    cfmakeraw(&settings);
    return settings;
}

static void settings_set(const struct termios *settings)
{
    CHECK(tcsetattr(0, TCSANOW, settings) == 0);
}

static void settings_restore(void)
{
    CHECK(tcsetattr(0, TCSAFLUSH, &original) == 0);
}

static void settings_are(const struct termios *expected)
{
    struct termios now;
    CHECK(tcgetattr(0, &now) == 0);
    CHECK(memcmp(&now, expected, sizeof(now)) == 0);
}

static void read_is(const char *expected)
{
    char bytes[32];
    size_t length = strlen(expected);
    CHECK(read(0, bytes, sizeof(bytes)) == (long)length);
    CHECK(memcmp(bytes, expected, length) == 0);
}

// A terminal starts out reading lines, with its keys and its echo on
static void test_defaults(void)
{
    CHECK(original.c_lflag & ICANON);
    CHECK(original.c_lflag & ECHO);
    CHECK(original.c_lflag & ISIG);
    CHECK(original.c_iflag & ICRNL);
    CHECK(original.c_cc[VMIN] == 1);
    CHECK(original.c_cc[VINTR] == CTRL_C);
    CHECK(original.c_cc[VEOF] == CTRL_D);
    settings_are(&original);
}

// What was set is what comes back, whole
static void test_settings_kept(void)
{
    struct termios settings = settings_raw();
    settings.c_cc[VTIME] = 7;
    settings_set(&settings);
    settings_are(&settings);
    settings_restore();
    settings_are(&original);
}

static void test_make_raw(void)
{
    struct termios settings = settings_raw();
    CHECK((settings.c_lflag & (ICANON | ECHO | ISIG | IEXTEN)) == 0);
    CHECK((settings.c_iflag & (ICRNL | IXON)) == 0);
    CHECK((settings.c_oflag & OPOST) == 0);
    CHECK((settings.c_cflag & CSIZE) == CS8);
    CHECK(settings.c_cc[VMIN] == 1);
    CHECK(settings.c_cc[VTIME] == 0);
}

// No Enter needed, and a read takes what it asked for and leaves the rest
static void test_raw_reads_bytes(void)
{
    struct termios settings = settings_raw();
    settings_set(&settings);

    char bytes[8];
    press("abc");
    CHECK(read(0, bytes, 1) == 1 && bytes[0] == 'a');
    CHECK(read(0, bytes, sizeof(bytes)) == 2);
    CHECK(memcmp(bytes, "bc", 2) == 0);

    // A newline ends nothing here
    press("a\nb");
    read_is("a\nb");
    settings_restore();
}

// Nothing is a key with a meaning: this test would be dead if Ctrl+C still signalled
static void test_raw_delivers_control_bytes(void)
{
    static const char keys[] = {
        CTRL_C, CTRL_BACKSLASH, CTRL_Z, CTRL_D, CTRL_U, BACKSPACE, '\b', ESCAPE, '\t',
    };

    struct termios settings = settings_raw();
    settings.c_cc[VMIN] = sizeof(keys);
    settings_set(&settings);

    for (size_t i = 0; i < sizeof(keys); i++)
        press_byte(keys[i]);

    char bytes[sizeof(keys)];
    CHECK(read(0, bytes, sizeof(bytes)) == (long)sizeof(keys));
    CHECK(memcmp(bytes, keys, sizeof(keys)) == 0);
    settings_restore();
}

// ICRNL turns Enter into a newline. A raw program asked not to have that
static void test_enter(void)
{
    press("line");
    press_byte('\r');
    read_is("line\n");

    struct termios settings = settings_raw();
    settings_set(&settings);
    press_byte('\r');
    read_is("\r");

    settings.c_iflag |= ICRNL;
    settings_set(&settings);
    press_byte('\r');
    read_is("\n");
    settings_restore();
}

// ISIG on its own: a program can read bytes and still be stopped with Ctrl+C
static void test_raw_with_signals(void)
{
    pid_t child = fork_flushed();
    CHECK(child >= 0);
    if (child == 0)
        for (;;)
            pause();
    CHECK(setpgid(child, child) == 0);

    struct termios settings = settings_raw();
    settings.c_lflag |= ISIG;
    settings_set(&settings);

    CHECK(tcsetpgrp(0, child) == 0);
    press_byte(CTRL_C);
    child_killed_by(child, SIGINT);
    CHECK(tcsetpgrp(0, getpgrp()) == 0);
    settings_restore();
}

// The line is still read and edited. ./run-tests.sh looks for "not on screen" in the boot's log
static void test_no_echo(void)
{
    struct termios settings = original;
    settings.c_lflag &= ~ECHO;
    settings_set(&settings);

    press("not on screen\n");
    read_is("not on screen\n");

    press("ab");
    press_byte(BACKSPACE);
    press("c\n");
    read_is("ac\n");
    settings_restore();
}

// VMIN 0: a read never sleeps, the way a program polls for a key
static void test_vmin_zero(void)
{
    struct termios settings = settings_raw();
    settings.c_cc[VMIN] = 0;
    settings_set(&settings);

    char byte;
    CHECK(read(0, &byte, 1) == 0);
    press("a");
    CHECK(read(0, &byte, 1) == 1 && byte == 'a');
    CHECK(read(0, &byte, 1) == 0);
    settings_restore();
}

// VMIN 3: a read sleeps for all three, unless it asked for fewer
static void test_vmin_many(void)
{
    struct termios settings = settings_raw();
    settings.c_cc[VMIN] = 3;
    settings_set(&settings);

    pid_t child = fork_flushed();
    CHECK(child >= 0);
    if (child == 0)
    {
        read_is("abc");
        child_done();
    }

    int status;
    press("ab");
    spin();
    CHECK(waitpid(child, &status, WNOHANG) == 0);
    press("c");
    child_passed(child);

    char bytes[2];
    press("xy");
    CHECK(read(0, bytes, sizeof(bytes)) == 2);
    CHECK(memcmp(bytes, "xy", 2) == 0);
    settings_restore();
}

// The line being typed is not lost, and a read asleep on it wakes up
static void test_switch_to_raw(void)
{
    struct termios settings = settings_raw();

    press("half");
    settings_set(&settings);
    read_is("half");
    settings_restore();

    pid_t child = fork_flushed();
    CHECK(child >= 0);
    if (child == 0)
    {
        read_is("half");
        child_done();
    }

    int status;
    press("half");
    spin();
    CHECK(waitpid(child, &status, WNOHANG) == 0);
    settings_set(&settings);
    child_passed(child);
    settings_restore();
}

// TCSAFLUSH drops lines and the line being typed. The other two keep them
static void test_when(void)
{
    press("read by nobody\n");
    press("half a line");
    CHECK(tcsetattr(0, TCSAFLUSH, &original) == 0);
    press("kept\n");
    read_is("kept\n");

    press("kept\n");
    CHECK(tcsetattr(0, TCSADRAIN, &original) == 0);
    read_is("kept\n");

    press("kept\n");
    CHECK(tcsetattr(0, TCSANOW, &original) == 0);
    read_is("kept\n");

    errno = 0;
    CHECK(tcsetattr(0, WHEN_NOT_VALID, &original) == -1 && errno == EINVAL);
}

static void test_not_a_terminal(void)
{
    int fds[2];
    struct termios settings;

    CHECK(pipe(fds) == 0);
    errno = 0;
    CHECK(tcgetattr(fds[0], &settings) == -1 && errno == ENOTTY);
    errno = 0;
    CHECK(tcsetattr(fds[0], TCSANOW, &original) == -1 && errno == ENOTTY);
    close(fds[0]);
    close(fds[1]);
}

// Changing the settings from the background stops the group, unless it ignores the signal
static void test_background(void)
{
    struct termios settings = settings_raw();

    pid_t child = fork_flushed();
    CHECK(child >= 0);
    if (child == 0)
    {
        signal(SIGTTOU, SIG_DFL); // this test ignores it, and fork passes that on
        setpgid(0, 0);
        tcsetattr(0, TCSANOW, &settings);
        _exit(1); // not reached: it is killed while stopped
    }
    setpgid(child, child);

    int status = 0;
    CHECK(waitpid(child, &status, WUNTRACED) == child);
    CHECK(WIFSTOPPED(status) && WSTOPSIG(status) == SIGTTOU);
    settings_are(&original);
    CHECK(kill(child, SIGKILL) == 0);
    child_killed_by(child, SIGKILL);

    child = fork_flushed();
    CHECK(child >= 0);
    if (child == 0)
    {
        signal(SIGTTOU, SIG_IGN);
        setpgid(0, 0);
        CHECK(tcsetattr(0, TCSANOW, &settings) == 0);
        child_done();
    }
    setpgid(child, child);
    child_passed(child);
    settings_are(&settings);
    settings_restore();

    // Reading them is allowed from anywhere
    child = fork_flushed();
    CHECK(child >= 0);
    if (child == 0)
    {
        struct termios now;
        setpgid(0, 0);
        CHECK(tcgetattr(0, &now) == 0);
        child_done();
    }
    setpgid(child, child);
    child_passed(child);
}

int main(void)
{
    // A case hands the foreground out, so it asks for it back from the background
    signal(SIGTTOU, SIG_IGN);

    CHECK(tcgetattr(0, &original) == 0);

    test_defaults();
    test_settings_kept();
    test_make_raw();
    test_raw_reads_bytes();
    test_raw_delivers_control_bytes();
    test_enter();
    test_raw_with_signals();
    test_no_echo();
    test_vmin_zero();
    test_vmin_many();
    test_switch_to_raw();
    test_when();
    test_not_a_terminal();
    test_background();

    settings_are(&original);

    // Every pipe is closed again: the next free fd is 3
    int fd = dup(0);
    CHECK(fd == 3);
    close(fd);
    return test_status();
}
