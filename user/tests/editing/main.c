#include <sys/ioctl.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <errno.h>
#include <test.h>

/*
 * Typing into the interactive shell. /bin/sh runs with the console on stdin
 * and stderr, so it is interactive, and stdout into a pipe, so what a command
 * printed can be compared. The keys go in through TIOCSTI: its bytes are what
 * the console sends for a key, escape sequences included.
 *
 * The shell's terminal settings say where it is. Raw means it is at a prompt,
 * so a key pressed then is for the line. A command's output means the line
 * was finished, and the shell can only go raw again at the next prompt.
 *
 * What the screen shows is not checked here: text on wrapped lines and accents
 * (TIOCSTI takes each byte as a whole character) were checked by hand.
 */

#define SPINS 1000000 // time for the shell to get somewhere
#define OUTPUT_MAX 1024
#define HISTORY_PAST_OLDEST 40 // the shell keeps 32 lines
#define CTRL_C "\x03"
#define CTRL_D "\x04"
#define CTRL_U "\x15"
#define CTRL_Z "\x1A"
#define CTRL_BACKSLASH "\x1C"
#define BACKSPACE "\x7F"
#define UP "\x1b[A"
#define DOWN "\x1b[B"
#define RIGHT "\x1b[C"
#define LEFT "\x1b[D"
#define HOME "\x1b[H"
#define END "\x1b[F"
#define DELETE "\x1b[3~"

static struct termios original;
static pid_t shell;
static int shell_output;

static char output[OUTPUT_MAX];
static size_t output_length;

static void spin(void)
{
    for (volatile int i = 0; i < SPINS; i++)
        ;
}

static void press(const char *keys)
{
    for (; *keys != '\0'; keys++)
        CHECK(ioctl(0, TIOCSTI, keys) == 0);
}

// Where expected starts in what was printed so far, or NULL
static char *output_find(const char *expected)
{
    size_t length = strlen(expected);

    for (size_t i = 0; i + length <= output_length; i++)
        if (memcmp(output + i, expected, length) == 0)
            return output + i;

    return NULL;
}

// Only output coming later counts next time. Waits for it: a shell that never prints fails by timeout
static void output_has(const char *expected)
{
    for (;;)
    {
        char *found = output_find(expected);
        if (found != NULL)
        {
            char *rest = found + strlen(expected);
            output_length -= rest - output;
            memmove(output, rest, output_length);
            return;
        }

        CHECK(output_length < sizeof(output));
        long got = read(shell_output, output + output_length, sizeof(output) - output_length);
        CHECK(got > 0);
        if (got <= 0)
            return;

        output_length += got;
    }
}

static int settings_raw(void)
{
    struct termios now;
    CHECK(tcgetattr(0, &now) == 0);
    return !(now.c_lflag & ICANON);
}

// However long it takes: from the disk boot, /bin/sh is read from ext2 first.
// A shell that is gone never gets there, and the case fails at once
static void prompt_reached(void)
{
    int status;

    while (!settings_raw())
    {
        if (waitpid(shell, &status, WNOHANG) != 0)
        {
            CHECK(!"the shell ended before its prompt");
            return;
        }
        spin();
    }
}

// Types a line at the next prompt, keys and all, and compares what running it printed
static void line_prints(const char *keys, const char *expected)
{
    prompt_reached();
    press(keys);
    output_has(expected);
}

static void shell_start(void)
{
    int fds[2], go[2];
    CHECK(pipe(fds) == 0);
    CHECK(pipe(go) == 0);

    fflush(stdout);
    shell = fork();
    CHECK(shell >= 0);
    if (shell == 0)
    {
        char byte;
        char *argv[] = {"sh", NULL};

        close(fds[0]);
        dup2(fds[1], 1);
        close(fds[1]);
        setpgid(0, 0);

        // It takes the terminal first thing, which would stop it from the background
        close(go[1]);
        read(go[0], &byte, 1);
        close(go[0]);

        execv("/bin/sh", argv);
        _exit(127);
    }

    close(fds[1]);
    shell_output = fds[0];

    CHECK(setpgid(shell, shell) == 0);
    CHECK(tcsetpgrp(0, shell) == 0);
    close(go[0]);
    close(go[1]);
}

static void test_typing(void)
{
    line_prints("echo one\r", "one\n");
}

static void test_moving(void)
{
    line_prints("echo ac" LEFT "b\r", "abc\n");
    line_prints("echo abc" LEFT LEFT RIGHT "x\r", "abxc\n");
    line_prints("cho ab" HOME "e" END "c\r", "abc\n");

    // Nothing to the left of the start, nothing to the right of the end
    line_prints("echo ab" RIGHT RIGHT "c" HOME LEFT LEFT "\r", "abc\n");
}

static void test_erasing(void)
{
    line_prints("echo abd" BACKSPACE "c\r", "abc\n");
    line_prints("echo axc" LEFT BACKSPACE "b\r", "abc\n");
    line_prints("xecho ab" HOME DELETE "\r", "ab\n");
    line_prints("xecho ab" HOME CTRL_D "\r", "ab\n");

    // Ctrl+U takes what is left of the cursor and keeps the rest
    line_prints("junk" CTRL_U "echo ab\r", "ab\n");
    line_prints("junk echo ab" LEFT LEFT LEFT LEFT LEFT LEFT LEFT CTRL_U "\r", "ab\n");

    // Nothing to erase
    line_prints(BACKSPACE DELETE "echo ab" BACKSPACE BACKSPACE DELETE "cd\r", "cd\n");
}

static void test_history(void)
{
    line_prints("echo first\r", "first\n");
    line_prints("echo second\r", "second\n");

    line_prints(UP "\r", "second\n");
    line_prints(UP UP UP "\r", "first\n");
    line_prints(UP UP DOWN "\r", "first\n");

    // A shorter line replaces a longer one whole
    line_prints("echo a much longer line" UP "\r", "first\n");

    // A recalled line can be edited, and running it adds a new one
    line_prints(UP BACKSPACE "x\r", "firsx\n");
    line_prints(UP "\r", "firsx\n");

    // Past the newest is the line being typed, as it was left
    line_prints("echo draft" UP UP DOWN DOWN "\r", "draft\n");
    line_prints("echo draft2" DOWN DOWN "\r", "draft2\n");
}

// The oldest line is as far as it goes
static void test_history_oldest(void)
{
    prompt_reached();
    for (int i = 0; i < HISTORY_PAST_OLDEST; i++)
        press(UP);

    press(CTRL_U "echo still here\r");
    output_has("still here\n");
}

// At a prompt Ctrl+C drops the line, and the other two keys do nothing
static void test_prompt_keys(void)
{
    prompt_reached();
    press("not run" CTRL_C);
    line_prints("echo $?\r", "130\n");

    line_prints("echo a" CTRL_Z CTRL_BACKSLASH "b\r", "ab\n");
    line_prints("echo $?\r", "0\n");
}

// A program gets the terminal as the shell found it, and Ctrl+C reaches the program
static void test_program_settings(void)
{
    prompt_reached();
    press("cat\r");

    // The shell gives the terminal back as it found it before starting cat
    while (settings_raw())
        spin();

    struct termios now;
    CHECK(tcgetattr(0, &now) == 0);
    CHECK(memcmp(&now, &original, sizeof(now)) == 0);

    press("typed\n");
    output_has("typed\n");
    press(CTRL_C);
    line_prints("echo $?\r", "130\n");
}

// Ctrl+D on an empty line leaves, with the terminal as the shell found it
static void test_leaving(void)
{
    prompt_reached();
    press(CTRL_D);

    int status = -1;
    CHECK(waitpid(shell, &status, 0) == shell);
    CHECK(WIFEXITED(status) && WEXITSTATUS(status) == 0);
    close(shell_output);

    struct termios now;
    CHECK(tcgetattr(0, &now) == 0);
    CHECK(memcmp(&now, &original, sizeof(now)) == 0);

    CHECK(tcsetpgrp(0, getpgrp()) == 0);
}

int main(void)
{
    // The shell is handed the terminal, so this asks for it back from the background
    signal(SIGTTOU, SIG_IGN);

    CHECK(tcgetattr(0, &original) == 0);

    shell_start();
    test_typing();
    test_moving();
    test_erasing();
    test_history();
    test_history_oldest();
    test_prompt_keys();
    test_program_settings();
    test_leaving();

    // Every pipe is closed again: the next free fd is 3
    int fd = dup(0);
    CHECK(fd == 3);
    close(fd);
    return test_status();
}
