#include <fcntl.h>
#include <spawn.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#include <test.h>

extern char **environ; // POSIX: the program declares it, no header has to

/*
 * BusyBox, the commands (ports/busybox/, roadmap D10g): /bin/busybox runs each
 * case's applet as "busybox <applet> <arguments>", with the case's input on
 * stdin, and must print exactly the output given and exit with the status
 * given. stderr stays the console's, so mlibc's "missing sysdep" warnings and
 * busybox's own messages show in the log next to the failing case.
 *
 * The cases cover what each applet needs below it: pipes and plain reads and
 * writes for the text tools, the clock, directories, files on the test disk,
 * and ash running a command line.
 *
 * A case with waits_for set is parked: it needs calls the kernel or our mlibc
 * sysdeps do not have yet, and is skipped (counted, not run) until they land.
 * Those cases are the to-do list for the next syscalls.
 */

#define BUSYBOX_PATH "/bin/busybox"
#define OUTPUT_MAX 512
#define ARGS_MAX 8

typedef struct busybox_case
{
    const char *args[ARGS_MAX]; // the applet and its arguments, NULL-terminated
    const char *input;          // stdin, or NULL for none (an empty pipe)
    const char *output;         // stdout, exactly
    int status;
    const char *waits_for; // the missing calls this case needs; NULL = runs
} busybox_case_t;

static const busybox_case_t cases[] = {
    // Output only
    {{"echo", "hello", "world"}, NULL, "hello world\n", 0, NULL},
    {{"echo", "-n", "a"}, NULL, "a", 0, NULL},
    {{"printf", "%s-%d\\n", "x", "5"}, NULL, "x-5\n", 0, NULL}, // checks stdout with F_GETFL first
    {{"seq", "1", "3"}, NULL, "1\n2\n3\n", 0, NULL},
    {{"seq", "-s", ",", "1", "5"}, NULL, "1,2,3,4,5\n", 0, NULL},
    {{"basename", "/usr/tests/run"}, NULL, "run\n", 0, NULL},
    {{"dirname", "/usr/tests/run"}, NULL, "/usr/tests\n", 0, NULL},
    {{"true"}, NULL, "", 0, NULL},
    {{"false"}, NULL, "", 1, NULL},
    {{"test", "3", "-gt", "2"}, NULL, "", 0, NULL},
    {{"test", "-d", "/usr/tests"}, NULL, "", 0, NULL},
    // By path: -i empties PATH too, and mlibc's execvp has no default PATH
    {{"env", "-i", "A=1", BUSYBOX_PATH, "env"}, NULL, "A=1\n", 0, NULL},
    {{"date", "-u", "-d", "@0", "+%Y-%m-%d %H:%M:%S"}, NULL, "1970-01-01 00:00:00\n", 0, NULL},

    // Text from stdin
    {{"cat"}, "one\ntwo\n", "one\ntwo\n", 0, NULL},
    {{"sort"}, "b\na\nc\n", "a\nb\nc\n", 0, NULL},
    {{"sort", "-n", "-r"}, "2\n10\n1\n", "10\n2\n1\n", 0, NULL},
    {{"uniq"}, "a\na\nb\n", "a\nb\n", 0, NULL},
    {{"wc", "-l"}, "a\nb\nc\n", "3\n", 0, NULL},
    {{"head", "-n", "2"}, "1\n2\n3\n", "1\n2\n", 0, NULL},
    {{"tail", "-n", "1"}, "1\n2\n3\n", "3\n", 0, NULL},
    {{"cut", "-d", ":", "-f", "2"}, "a:b:c\n", "b\n", 0, NULL},
    {{"tr", "a-z", "A-Z"}, "abc\n", "ABC\n", 0, NULL},
    {{"grep", "-c", "b"}, "ab\nb\nc\n", "2\n", 0, NULL},
    {{"grep", "x"}, "a\nb\n", "", 1, NULL}, // no match
    {{"sed", "s/o/0/g"}, "foo\n", "f00\n", 0, NULL},
    // Busybox's small od (no CONFIG_DESKTOP): the traditional options only, no -A or -t
    {{"od", "-b"}, "AB", "0000000 101 102\n0000002\n", 0, NULL},
    {{"md5sum"}, "abc\n", "0bee89b07a248e27c83fc3d5951213c1  -\n", 0, NULL},
    {{"sha256sum"}, "abc\n", "edeaaff3f1774ad2888673770c6d64097e391bc362d7d6fb34982ddf0efd18cb  -\n", 0, NULL},
    {{"xargs", "echo"}, "a b\nc\n", "a b c\n", 0, NULL},

    // Directories and files: /usr holds only tests in both boots; /mnt is the writable test disk
    {{"ls", "/usr"}, NULL, "tests\n", 0, NULL},
    {{"mkdir", "/mnt/busybox"}, NULL, "", 0, NULL},
    // ash saves the fd a redirection replaces with F_DUPFD_CLOEXEC, at 10 and up
    {{"sh", "-c", "echo made > /mnt/busybox/file"}, NULL, "", 0, NULL},
    {{"cat", "/mnt/busybox/file"}, NULL, "made\n", 0, NULL},
    {{"cp", "/mnt/busybox/file", "/mnt/busybox/copy"}, NULL, "", 0, NULL},
    {{"cmp", "/mnt/busybox/file", "/mnt/busybox/copy"}, NULL, "", 0, NULL},
    {{"mv", "/mnt/busybox/copy", "/mnt/busybox/moved"}, NULL, "", 0, "rename"},
    {{"ls", "/mnt/busybox"}, NULL, "file\nmoved\n", 0, "rename"},
    {{"touch", "/mnt/busybox/new"}, NULL, "", 0, "utimensat"},
    {{"find", "/mnt/busybox", "-name", "n*"}, NULL, "/mnt/busybox/new\n", 0, "utimensat"},
    // -f: whichever of these the parked cases above did not make is skipped, so rmdir finds it empty
    {{"rm", "-f", "/mnt/busybox/file", "/mnt/busybox/copy", "/mnt/busybox/moved", "/mnt/busybox/new"},
     NULL, "", 0, NULL},
    {{"rmdir", "/mnt/busybox"}, NULL, "", 0, NULL},
    {{"test", "-e", "/mnt/busybox"}, NULL, "", 1, NULL},

    // The shell: arithmetic, a pipeline, a loop, an exit status
    {{"sh", "-c", "echo $((6 * 7))"}, NULL, "42\n", 0, NULL},
    {{"sh", "-c", "{ echo b; echo a; } | busybox sort"}, NULL, "a\nb\n", 0, NULL},
    {{"sh", "-c", "for i in 1 2 3; do echo -n $i; done; echo"}, NULL, "123\n", 0, NULL},
    {{"sh", "-c", "exit 3"}, NULL, "", 3, NULL},
};

/*
 * Runs busybox with the case's arguments: input on stdin through one pipe,
 * stdout read back through another until busybox closes it. The inputs are
 * small, so writing all of it before reading cannot fill the pipe.
 * Returns the exit status (-1 if busybox did not start or exit normally);
 * output receives up to size - 1 bytes, '\0'-terminated.
 */
static int run(const busybox_case_t *c, char *output, size_t size)
{
    char *argv[ARGS_MAX + 2] = {BUSYBOX_PATH};
    for (int i = 0; i < ARGS_MAX && c->args[i] != NULL; i++)
        argv[i + 1] = (char *)c->args[i];

    int in[2], out[2];
    if (pipe(in) != 0 || pipe(out) != 0)
    {
        printf("pipe failed: %s\n", strerror(errno));
        return -1;
    }

    posix_spawn_file_actions_t actions;
    posix_spawn_file_actions_init(&actions);
    posix_spawn_file_actions_adddup2(&actions, in[0], 0);
    posix_spawn_file_actions_adddup2(&actions, out[1], 1);
    posix_spawn_file_actions_addclose(&actions, in[1]);
    posix_spawn_file_actions_addclose(&actions, out[0]);

    pid_t pid;
    int error = posix_spawn(&pid, BUSYBOX_PATH, &actions, NULL, argv, environ);
    posix_spawn_file_actions_destroy(&actions);
    close(in[0]);
    close(out[1]);
    if (error != 0)
    {
        printf("could not start %s: %s\n", BUSYBOX_PATH, strerror(error));
        close(in[1]);
        close(out[0]);
        return -1;
    }

    if (c->input != NULL)
        write(in[1], c->input, strlen(c->input));
    close(in[1]);

    size_t length = 0;
    ssize_t n;
    while (length < size - 1 && (n = read(out[0], output + length, size - 1 - length)) > 0)
        length += n;
    output[length] = '\0';
    close(out[0]);

    int status;
    if (waitpid(pid, &status, 0) != pid || !WIFEXITED(status))
        return -1;
    return WEXITSTATUS(status);
}

// Prints text with its newlines and tabs visible, for a mismatch's report
static void print_escaped(const char *text)
{
    for (; *text != '\0'; text++)
    {
        if (*text == '\n')
            printf("\\n");
        else if (*text == '\t')
            printf("\\t");
        else
            putchar(*text);
    }
}

int main(void)
{
    unsigned int parked = 0;
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
    {
        const busybox_case_t *c = &cases[i];
        if (c->waits_for != NULL)
        {
            parked++;
            continue;
        }

        char output[OUTPUT_MAX];
        int status = run(c, output, sizeof(output));

        int output_ok = strcmp(output, c->output) == 0;
        if (status != c->status || !output_ok)
        {
            printf("case \"busybox");
            for (int a = 0; a < ARGS_MAX && c->args[a] != NULL; a++)
                printf(" %s", c->args[a]);
            printf("\": status %d (expected %d), output \"", status, c->status);
            print_escaped(output);
            printf("\" (expected \"");
            print_escaped(c->output);
            printf("\")\n");
        }
        CHECK(status == c->status && output_ok);
    }

    if (parked > 0)
        printf("busybox: %u cases parked until their calls land\n", parked);

    return test_status();
}
