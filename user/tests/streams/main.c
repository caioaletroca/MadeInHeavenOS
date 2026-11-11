#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <test.h>

/*
 * Streams on files: fopen's modes, fdopen, fileno and fclose, and the list of
 * open streams that fflush(NULL) and exit walk.
 *
 * Files go on the writable test disk (/mnt) in both boots, each removed
 * again. A file is not a terminal, so its stream is fully buffered: nothing
 * reaches the file before a flush, a close or exit.
 */

#define DIR "/mnt"
#define FILE_A DIR "/streams-a"
#define FILE_B DIR "/streams-b"
#define FILE_C DIR "/streams-c"
#define MISSING DIR "/streams-missing"

// The file's whole contents into text (cut to size), or "" if it cannot be read
static const char *contents(const char *path, char *text, size_t size)
{
    text[0] = '\0';
    int fd = open(path, O_RDONLY);
    if (fd < 0)
        return text;

    size_t length = 0;
    ssize_t got;
    while (length + 1 < size && (got = read(fd, text + length, size - 1 - length)) > 0)
        length += (size_t)got;
    text[length] = '\0';
    close(fd);
    return text;
}

static long size_of(const char *path)
{
    struct stat st;
    return stat(path, &st) == 0 ? (long)st.st_size : -1;
}

// Runs body in a child that ends with exit (not _exit), so exit's flush runs: its status
static int in_child(void (*body)(void))
{
    fflush(stdout);
    pid_t pid = fork();
    if (pid == 0)
    {
        body();
        exit(test_status());
    }

    int status;
    if (pid < 0 || waitpid(pid, &status, 0) != pid || !WIFEXITED(status))
        return -1;
    return WEXITSTATUS(status);
}

static void test_write_and_read(void)
{
    char text[64];

    // "w" creates; the text waits in the buffer until a flush
    FILE *f = fopen(FILE_A, "w");
    CHECK(f != NULL);
    CHECK(fputs("hello\n", f) >= 0);
    CHECK(size_of(FILE_A) == 0); // a file, so fully buffered: "\n" sends nothing yet
    CHECK(fflush(f) == 0);
    CHECK(size_of(FILE_A) == 6);
    CHECK(fputs("more", f) >= 0);
    CHECK(fclose(f) == 0); // the close flushes the rest
    CHECK(strcmp(contents(FILE_A, text, sizeof(text)), "hello\nmore") == 0);

    // "r" reads from the start, to end of file
    f = fopen(FILE_A, "r");
    CHECK(f != NULL);
    CHECK(fgets(text, sizeof(text), f) != NULL && strcmp(text, "hello\n") == 0);
    CHECK(fgets(text, sizeof(text), f) != NULL && strcmp(text, "more") == 0);
    CHECK(fgetc(f) == EOF && feof(f) && !ferror(f));
    CHECK(fclose(f) == 0);

    // "a" adds at the end; "rb": 'b' changes nothing
    f = fopen(FILE_A, "a");
    CHECK(f != NULL && fputs("!\n", f) >= 0 && fclose(f) == 0);
    CHECK(strcmp(contents(FILE_A, text, sizeof(text)), "hello\nmore!\n") == 0);
    f = fopen(FILE_A, "rb");
    CHECK(f != NULL && fgetc(f) == 'h' && fclose(f) == 0);

    // "r+" opens without emptying, and reads
    f = fopen(FILE_A, "r+");
    CHECK(f != NULL && fgetc(f) == 'h' && fclose(f) == 0);
    CHECK(size_of(FILE_A) == 12);

    // "w" empties an existing file at once
    f = fopen(FILE_A, "w");
    CHECK(f != NULL && size_of(FILE_A) == 0 && fclose(f) == 0);

    // The wrong direction: EOF and the error indicator
    f = fopen(FILE_A, "r");
    CHECK(f != NULL);
    CHECK(fputc('x', f) == EOF && ferror(f));
    CHECK(fclose(f) == 0);
    f = fopen(FILE_A, "w");
    CHECK(f != NULL);
    CHECK(fgetc(f) == EOF && ferror(f));
    CHECK(fclose(f) == 0);

    CHECK(unlink(FILE_A) == 0);
}

static void test_open_errors(void)
{
    // "r" and "r+" never create
    errno = 0;
    CHECK(fopen(MISSING, "r") == NULL && errno == ENOENT);
    errno = 0;
    CHECK(fopen(MISSING, "r+") == NULL && errno == ENOENT);
    CHECK(size_of(MISSING) == -1);

    // "wx": only a new file
    FILE *f = fopen(FILE_A, "wx");
    CHECK(f != NULL && fclose(f) == 0);
    errno = 0;
    CHECK(fopen(FILE_A, "wx") == NULL && errno == EEXIST);
    errno = 0;
    CHECK(fopen(FILE_A, "w+x") == NULL && errno == EEXIST);
    CHECK(unlink(FILE_A) == 0);

    // A mode must start with r, w or a
    errno = 0;
    CHECK(fopen(FILE_A, "z") == NULL && errno == EINVAL);
    errno = 0;
    CHECK(fopen(FILE_A, "") == NULL && errno == EINVAL);
    errno = 0;
    CHECK(fopen(FILE_A, "+r") == NULL && errno == EINVAL);
    CHECK(size_of(FILE_A) == -1); // nothing was created
}

static void test_fdopen(void)
{
    int fds[2];
    CHECK(pipe(fds) == 0);

    FILE *out = fdopen(fds[1], "w");
    FILE *in = fdopen(fds[0], "r");
    CHECK(out != NULL && in != NULL);
    CHECK(fileno(out) == fds[1] && fileno(in) == fds[0]);

    // A pipe is no terminal: the line waits until the close, which also closes the fd
    CHECK(fprintf(out, "%d pipes\n", 2) == 8);
    CHECK(fclose(out) == 0);

    char text[32];
    CHECK(fgets(text, sizeof(text), in) != NULL && strcmp(text, "2 pipes\n") == 0);
    CHECK(fgetc(in) == EOF && feof(in)); // the write end is closed: end of file
    CHECK(fclose(in) == 0);
    errno = 0;
    CHECK(close(fds[0]) == -1 && errno == EBADF); // fclose closed it

    errno = 0;
    CHECK(fdopen(99, "r") == NULL && errno == EBADF);
    errno = 0;
    CHECK(fdopen(1, "q") == NULL && errno == EINVAL);

    CHECK(fileno(stdin) == 0 && fileno(stdout) == 1 && fileno(stderr) == 2);
}

// Every fclose gives its fd back: more opens than a process has fds
static void test_no_leak(void)
{
    FILE *f = fopen(FILE_A, "w");
    CHECK(f != NULL && fclose(f) == 0);

    int failures = 0;
    for (int i = 0; i < 100; i++)
    {
        f = fopen(FILE_A, "r");
        if (f == NULL || fclose(f) != 0)
            failures++;
    }
    CHECK(failures == 0);

    CHECK(unlink(FILE_A) == 0);
}

// Three streams written and never flushed, the middle one closed: exit still flushes the others
static void three_streams(void)
{
    FILE *a = fopen(FILE_A, "w");
    FILE *b = fopen(FILE_B, "w");
    FILE *c = fopen(FILE_C, "w");
    CHECK(a != NULL && b != NULL && c != NULL);
    fputs("a", a);
    fputs("b", b);
    fputs("c", c);
    CHECK(fclose(b) == 0); // unlinked from the middle of the list
    fputs("A", a);
    fputs("C", c);
    CHECK(size_of(FILE_A) == 0 && size_of(FILE_C) == 0); // still in their buffers
}

// The newest stream is the list's head: closing it first, and the oldest last
static void head_and_tail(void)
{
    FILE *a = fopen(FILE_A, "w");
    FILE *b = fopen(FILE_B, "w");
    CHECK(a != NULL && b != NULL);
    fputs("1", a);
    fputs("2", b);
    CHECK(fclose(b) == 0);
    FILE *c = fopen(FILE_C, "w"); // the new head, in front of a
    CHECK(c != NULL);
    fputs("3", c);
    CHECK(fclose(a) == 0); // the tail of the opened ones
    fputs("4", c);
}

static void test_exit_flushes(void)
{
    char text[16];

    CHECK(in_child(three_streams) == 0);
    CHECK(strcmp(contents(FILE_A, text, sizeof(text)), "aA") == 0);
    CHECK(strcmp(contents(FILE_B, text, sizeof(text)), "b") == 0);
    CHECK(strcmp(contents(FILE_C, text, sizeof(text)), "cC") == 0);

    CHECK(in_child(head_and_tail) == 0);
    CHECK(strcmp(contents(FILE_A, text, sizeof(text)), "1") == 0);
    CHECK(strcmp(contents(FILE_B, text, sizeof(text)), "2") == 0);
    CHECK(strcmp(contents(FILE_C, text, sizeof(text)), "34") == 0);

    CHECK(unlink(FILE_A) == 0 && unlink(FILE_B) == 0 && unlink(FILE_C) == 0);
}

// fclose(stdout): its fd goes back, and a file that gets fd 1 next never receives printf's text
static void stdout_closed(void)
{
    // Reports go to stderr from here (CHECK prints to stdout)
    if (fclose(stdout) != 0)
        _exit(10);

    int fd = open(FILE_A, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd != 1)
        _exit(11);

    errno = 0;
    if (printf("leaked\n") >= 0 || !ferror(stdout))
        _exit(12);
    if (fputs("leaked", stdout) != EOF || fflush(stdout) != 0)
        _exit(13);
    // exit's fflush(NULL) no longer finds stdout in the list
}

static void test_stdout_closed(void)
{
    char text[16];
    CHECK(in_child(stdout_closed) == 0);
    CHECK(size_of(FILE_A) == 0 && strcmp(contents(FILE_A, text, sizeof(text)), "") == 0);
    CHECK(unlink(FILE_A) == 0);
}

int main(void)
{
    test_write_and_read();
    test_open_errors();
    test_fdopen();
    test_no_leak();
    test_exit_flushes();
    test_stdout_closed();
    return test_status();
}
