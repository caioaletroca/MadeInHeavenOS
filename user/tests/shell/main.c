#include <sys/wait.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
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
 * Then pipelines: `|` between programs, with cat and the shell itself as the
 * readers, and builtins, each stage in a forked copy of the shell.
 */

#define PIPE_SIZE 4096 // the kernel's buffer: a script must fit, nobody reads it while it is written
#define OUTPUT_MAX 1024
#define BINARY_MAX 65536 // a program piped through cat
#define BINARY_PATH "/usr/tests/hello"

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
    {"an aborted program",
     "/usr/tests/malloc_abort\necho $?\n",
     "134\n",
     "free: invalid pointer or double free\nsh: /usr/tests/malloc_abort: killed by signal 6\n", 0},
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
    // The shell reads its script one byte at a time: the lines after a command stay in the pipe
    // for that command (dash too). Read ahead, cat would find nothing and "hello" would run.
    {"a command reads the rest of the script", "cat\nhello\necho after\n", "hello\necho after\n", "", 0},
    {"a nested shell takes its lines, then the outer goes on",
     "sh\necho inner\nexit 3\necho $?\n",
     "inner\n3\n", "", 0},

    // Pipelines
    {"a pipe into cat", "echo hello | cat\n", "hello\n", "", 0},
    {"a pipe without spaces", "echo hello|cat\n", "hello\n", "", 0},
    {"a chain of cats", "echo a b | cat | cat | cat\n", "a b\n", "", 0},
    {"the longest pipeline", "echo eight | cat | cat | cat | cat | cat | cat | cat\n", "eight\n", "", 0},
    {"a quoted or escaped | is a word", "echo 'x|y' \"|\" \\| | cat\n", "x|y | |\n", "", 0},
    {"a script piped into a shell", "echo echo nested | sh\n", "nested\n", "", 0},
    {"cat with files and -", "echo from stdin | cat /dev/null - /dev/null\n", "from stdin\n", "", 0},
    {"in order with the commands around it",
     "echo first\necho second | cat\necho third\n",
     "first\nsecond\nthird\n", "", 0},
    // POSIX: a pipeline's status is its last command's
    {"the last command's status",
     "echo x | /usr/tests/hello\necho $?\n/usr/tests/hello | cat\necho $?\n",
     "42\n0\n", "", 0},
    {"a command not found in a pipeline",
     "nope | cat\necho $?\necho hi | nope\necho $?\n",
     "0\n127\n",
     "sh: nope: command not found\nsh: nope: command not found\n", 0},
    {"cat's errors",
     "cat /nope /dev/null\necho $?\n",
     "1\n",
     "cat: /nope: No such file or directory\n", 0},
    {"| with a command missing",
     "| cat\necho a |\necho a || cat\necho still running\n",
     "still running\n",
     "sh: syntax error near '|'\nsh: syntax error near '|'\nsh: syntax error near '|'\n", 0},
    {"too many commands",
     "echo 9 | cat | cat | cat | cat | cat | cat | cat | cat\n",
     "",
     "sh: too many commands in a pipeline\n", 0},
    // Found but not a program: exec's EACCES (bash: 126 too)
    {"a directory as a command",
     "/usr/tests\necho $?\n",
     "126\n",
     "sh: /usr/tests: cannot run (error 13)\n", 0},
    // The shell's own buffer is empty at each fork: nothing comes out twice
    {"a builtin in a pipeline",
     "pwd\npwd | cat\necho $?\npwd | cat | cat\n",
     "/\n/\n0\n/\n", "", 0},
    // It runs in a copy of the shell: what it does to itself stays there (bash too)
    {"a builtin's changes stay in its copy",
     "echo x | cd /usr\necho $?\npwd\necho x | exit 3\necho $?\necho still running\n",
     "0\n/\n3\nstill running\n", "", 0},
    {"a builtin failing in a pipeline",
     "echo x | cd /nope\necho $?\n",
     "1\n",
     "cd: /nope: No such file or directory\n", 0},
    // Bigger than a pipe, so cat fills it and waits. Nothing but nope's stage may hold the read
    // end: its exit is what wakes cat, and SIGPIPE ends it silently, as in bash
    {"a reader that never starts",
     "cat " BINARY_PATH " | nope\necho $?\n",
     "127\n",
     "sh: nope: command not found\n", 0},
};

static char *shell_environ[] = {"PATH=/bin", "GREETING=hello there", NULL};

// Reads until EOF or size bytes; returns the count, or -1
static long read_all(int fd, char *data, size_t size)
{
    size_t done = 0;
    while (done < size)
    {
        ssize_t n = read(fd, data + done, size - done);
        if (n < 0)
            return -1;
        if (n == 0)
            break;
        done += (size_t)n;
    }
    return (long)done;
}

/*
 * Runs /bin/sh on script. Fills out (out_size bytes, *out_length set) and err
 * (OUTPUT_MAX, '\0'-terminated) and *pid, and returns the exit status, or -1.
 * The script is written and its write end closed before the spawn: the shell
 * inherits every fd, and a write end it held would keep its stdin from ever
 * reaching EOF. stdout is read to EOF, then stderr, then the wait; stderr
 * stays below a pipe's size, so the shell never waits for room in it.
 */
static int shell_run(const char *script, char *out, size_t out_size, long *out_length, char *err, pid_t *pid)
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
        *out_length = read_all(output[0], out, out_size);
        long err_length = read_all(errors[0], err, OUTPUT_MAX - 1);
        err[err_length > 0 ? err_length : 0] = '\0';
        int raw;
        if (waitpid(*pid, &raw, 0) == *pid && WIFEXITED(raw) && *out_length >= 0 && err_length >= 0)
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
    long length = 0;
    pid_t pid;
    int status = shell_run(c->script, out, sizeof(out) - 1, &length, err, &pid);
    out[length > 0 ? length : 0] = '\0';

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
    long length = 0;
    pid_t pid = -1;
    CHECK(shell_run("echo $$\n", out, sizeof(out) - 1, &length, err, &pid) == 0);
    out[length > 0 ? length : 0] = '\0';

    char want[16];
    snprintf(want, sizeof(want), "%d\n", pid);
    expect_text("$$", "stdout", out, want);
    expect_text("$$", "stderr", err, "");
}

// A program file, bigger than a pipe's buffer and full of '\0's, through three cats unchanged
static void test_binary(void)
{
    static char want[BINARY_MAX], got[BINARY_MAX], err[OUTPUT_MAX];
    int fd = open(BINARY_PATH, O_RDONLY);
    CHECK(fd >= 0);
    long size = read_all(fd, want, sizeof(want));
    close(fd);
    CHECK(size > PIPE_SIZE && size < BINARY_MAX);

    long length = 0;
    pid_t pid;
    CHECK(shell_run("cat " BINARY_PATH " | cat | cat\n", got, sizeof(got), &length, err, &pid) == 0);
    CHECK(length == size && memcmp(got, want, (size_t)size) == 0);
    expect_text("binary", "stderr", err, "");
}

int main(void)
{
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
        test_case(&cases[i]);
    test_pid();
    test_binary();

    // Every pipe is closed again: the next free fd is 3
    int fd = dup(0);
    CHECK(fd == 3);
    close(fd);
    return test_status();
}
