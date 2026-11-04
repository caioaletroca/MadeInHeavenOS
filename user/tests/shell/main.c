#include <sys/wait.h>
#include <unistd.h>
#include <spawn.h>
#include <string.h>
#include <errno.h>
#include <test.h>

/*
 * The shell, run on scripts: /bin/sh with stdin from a pipe holding the
 * script and stdout and stderr each into a pipe of their own. Each case
 * compares both outputs and the exit status exactly. Not a terminal, so the
 * shell is non-interactive (POSIX, and what bash and dash do): no banner, no
 * prompt, no history expansion, error messages on stderr, and its own output
 * flushed before the next command, so it stays in order with its programs'.
 */

#define PIPE_SIZE 4096 // the kernel's buffer: a script must fit, nobody reads it while it is written
#define OUTPUT_MAX 1024

typedef struct shell_case
{
    const char *name;
    const char *script;
    const char *out; // stdout, exactly
    const char *err; // stderr, exactly
    int status;      // the shell's exit status
} shell_case_t;

static const shell_case_t cases[] = {
    {"a command, no banner or prompt", "echo hello\n", "hello\n", "", 0},
    {"quotes and escapes", "echo 'a  b' \"c\\\"d\" e\\ f ''x\n", "a  b c\"d e f x\n", "", 0},
    {"variables", "echo $GREETING \"$GREETING\" [$UNSET] $\n", "hello there hello there [] $\n", "", 0},
    {"exit statuses in $?",
     "/usr/tests/hello\necho $?\nnope\necho $?\n/no/such\necho $?\necho $?\n",
     "42\n127\n127\n0\n",
     "sh: nope: command not found\nsh: /no/such: command not found\n", 0},
    // abort exits with 128 + SIGABRT until signals (C5): an exit to the shell, so no "killed by signal" line
    {"an aborted program",
     "/usr/tests/malloc_abort\necho $?\n",
     "134\n",
     "free: invalid pointer or double free\n", 0},
    // pwd's output waits in the shell's buffer (a pipe is fully buffered) unless it is flushed before echo runs
    {"builtins and programs in order",
     "pwd\ncd /usr/tests\npwd\necho between\ncd ..\npwd\n",
     "/\n/usr/tests\nbetween\n/usr\n", "", 0},
    {"failing builtins, the last status at EOF",
     "cd /nope\necho $?\ncd a b\n",
     "1\n",
     "cd: /nope: No such file or directory\ncd: too many arguments\n", 1},
    {"exit stops the script", "echo before\nexit 7\necho after\n", "before\n", "", 7},
    {"exit keeps the last status", "nope\nexit\n", "", "sh: nope: command not found\n", 127},
    {"blank lines, a last line without a newline", "\n   \n\techo tab\necho last", "tab\nlast\n", "", 0},
    {"no history expansion", "echo one\necho !! !1\n", "one\n!! !1\n", "", 0},
    {"a parse error skips one line", "echo 'open\necho next\n", "next\n", "sh: unmatched quote\n", 0},
    {"an empty script", "", "", "", 0},
};

static char *shell_environ[] = {"PATH=/bin", "GREETING=hello there", NULL};

// Reads until EOF or size - 1 bytes and ends the text with '\0'; returns the count, or -1
static long read_text(int fd, char *text, size_t size)
{
    size_t done = 0;
    while (done < size - 1)
    {
        ssize_t n = read(fd, text + done, size - 1 - done);
        if (n < 0)
            return -1;
        if (n == 0)
            break;
        done += (size_t)n;
    }
    text[done] = '\0';
    return (long)done;
}

/*
 * Runs /bin/sh on script. Fills out and err (OUTPUT_MAX each) and *pid, and
 * returns the exit status, or -1. The script is written and its write end
 * closed before the spawn: the shell inherits every fd, and a write end it
 * held would keep its stdin from ever reaching EOF. The outputs are read to
 * EOF before the wait; they stay below a pipe's size, so the shell never
 * waits for room.
 */
static int shell_run(const char *script, char *out, char *err, pid_t *pid)
{
    int input[2], output[2], errors[2];
    size_t length = strlen(script);
    if (length >= PIPE_SIZE || pipe(input) < 0)
        return -1;
    int written = length == 0 || write(input[1], script, length) == (ssize_t)length;
    close(input[1]);
    if (!written || pipe(output) < 0)
    {
        close(input[0]);
        return -1;
    }
    if (pipe(errors) < 0)
    {
        close(input[0]);
        close(output[0]);
        close(output[1]);
        return -1;
    }

    // Our own stdio goes back right after the spawn: until then nothing may print
    fflush(stdout);
    int saved[3];
    for (int fd = 0; fd < 3; fd++)
        saved[fd] = dup(fd);
    dup2(input[0], 0);
    dup2(output[1], 1);
    dup2(errors[1], 2);

    char *argv[] = {"/bin/sh", NULL};
    int error = posix_spawn(pid, "/bin/sh", NULL, NULL, argv, shell_environ);

    for (int fd = 0; fd < 3; fd++)
    {
        dup2(saved[fd], fd);
        close(saved[fd]);
    }
    close(input[0]);
    close(output[1]);
    close(errors[1]);

    int status = -1;
    if (error == 0)
    {
        int ok = read_text(output[0], out, OUTPUT_MAX) >= 0 && read_text(errors[0], err, OUTPUT_MAX) >= 0;
        int raw;
        if (waitpid(*pid, &raw, 0) == *pid && WIFEXITED(raw) && ok)
            status = WEXITSTATUS(raw);
    }
    close(output[0]);
    close(errors[0]);
    return status;
}

static void expect_text(const char *name, const char *what, const char *got, const char *want)
{
    if (strcmp(got, want) != 0)
        printf("  %s: %s was \"%s\", expected \"%s\"\n", name, what, got, want);
    CHECK(strcmp(got, want) == 0);
}

static void test_case(const shell_case_t *c)
{
    static char out[OUTPUT_MAX], err[OUTPUT_MAX];
    out[0] = err[0] = '\0';
    pid_t pid;
    int status = shell_run(c->script, out, err, &pid);

    if (status != c->status)
        printf("  %s: status %d, expected %d\n", c->name, status, c->status);
    CHECK(status == c->status);
    expect_text(c->name, "stdout", out, c->out);
    expect_text(c->name, "stderr", err, c->err);
}

// $$ is the shell's own pid: only known once it is spawned
static void test_pid(void)
{
    static char out[OUTPUT_MAX], err[OUTPUT_MAX];
    pid_t pid = -1;
    CHECK(shell_run("echo $$\n", out, err, &pid) == 0);

    char want[16];
    snprintf(want, sizeof(want), "%d\n", pid);
    expect_text("$$", "stdout", out, want);
    expect_text("$$", "stderr", err, "");
}

int main(void)
{
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
        test_case(&cases[i]);
    test_pid();

    // Every pipe is closed again: the next free fd is 3
    int fd = dup(0);
    CHECK(fd == 3);
    close(fd);
    return test_status();
}
