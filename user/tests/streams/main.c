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

// path holds exactly text
static void make_file(const char *path, const char *text)
{
    FILE *f = fopen(path, "w");
    CHECK(f != NULL && fputs(text, f) >= 0 && fclose(f) == 0);
}

static void test_fread(void)
{
    // 1000 bytes: more than one 512-byte buffer
    static unsigned char pattern[1000], got[1000];
    for (size_t i = 0; i < sizeof(pattern); i++)
        pattern[i] = (unsigned char)(i % 251);

    FILE *f = fopen(FILE_A, "w");
    CHECK(f != NULL && fwrite(pattern, 1, sizeof(pattern), f) == sizeof(pattern) && fclose(f) == 0);
    CHECK(size_of(FILE_A) == 1000);

    f = fopen(FILE_A, "r");
    CHECK(f != NULL);
    CHECK(fread(got, 1, sizeof(got), f) == sizeof(got) && memcmp(got, pattern, sizeof(got)) == 0);
    CHECK(!feof(f)); // all asked for was there: end of file not seen yet
    CHECK(fread(got, 1, 1, f) == 0 && feof(f) && !ferror(f));

    // Whole items only: 3 of 300 bytes fit, the last 100 are read but not counted
    rewind(f);
    CHECK(fread(got, 300, 4, f) == 3 && feof(f) && ftell(f) == 1000);

    // An ungetc byte comes first, then the buffer
    rewind(f);
    CHECK(fgetc(f) == pattern[0] && ungetc('X', f) == 'X');
    CHECK(fread(got, 1, 3, f) == 3 && got[0] == 'X' && got[1] == pattern[1] && got[2] == pattern[2]);

    CHECK(fread(got, 0, 5, f) == 0 && fread(got, 5, 0, f) == 0 && !ferror(f)); // nothing asked
    CHECK(fclose(f) == 0);

    // The wrong direction
    f = fopen(FILE_A, "a");
    CHECK(f != NULL && fread(got, 1, 1, f) == 0 && ferror(f) && fclose(f) == 0);

    CHECK(unlink(FILE_A) == 0);
}

static void test_seek_and_tell(void)
{
    char text[16];
    make_file(FILE_A, "hello\nmore!\n"); // 12 bytes

    FILE *f = fopen(FILE_A, "r");
    CHECK(f != NULL);
    CHECK(ftell(f) == 0);

    // The read took all 12 bytes, the program one: the stream is at 1, not at the fd's 12
    CHECK(fgetc(f) == 'h' && ftell(f) == 1);
    CHECK(ungetc('h', f) == 'h' && ftell(f) == 0);
    CHECK(fgetc(f) == 'h' && fgets(text, sizeof(text), f) != NULL && strcmp(text, "ello\n") == 0);
    CHECK(ftell(f) == 6);

    // SEEK_CUR counts from the stream: 6 + 2 is the 'r' of "more"
    CHECK(fseek(f, 2, SEEK_CUR) == 0 && fgetc(f) == 'r' && ftell(f) == 9);

    // SEEK_END, end of file, and a seek that clears it
    CHECK(fseek(f, -1, SEEK_END) == 0 && fgetc(f) == '\n');
    CHECK(fgetc(f) == EOF && feof(f));
    CHECK(fseek(f, 0, SEEK_SET) == 0 && !feof(f) && fgetc(f) == 'h');

    // Errors leave the stream where it was
    errno = 0;
    CHECK(fseek(f, -1, SEEK_SET) == -1 && errno == EINVAL);
    errno = 0;
    CHECK(fseek(f, 0, 9) == -1 && errno == EINVAL);
    CHECK(ftell(f) == 1 && fgetc(f) == 'e');

    // Past the end: allowed, and nothing there
    CHECK(fseek(f, 100, SEEK_SET) == 0 && ftell(f) == 100 && fgetc(f) == EOF);

    // rewind clears both indicators
    CHECK(fputc('x', f) == EOF && ferror(f));
    rewind(f);
    CHECK(!ferror(f) && !feof(f) && ftell(f) == 0);
    CHECK(fclose(f) == 0);

    CHECK(unlink(FILE_A) == 0);
}

static void test_read_and_write(void)
{
    char text[16];

    // "w+": pending output counts in ftell; a seek turns writing into reading
    FILE *f = fopen(FILE_A, "w+");
    CHECK(f != NULL);
    CHECK(fputs("abc", f) >= 0 && ftell(f) == 3);
    CHECK(fseek(f, 0, SEEK_SET) == 0 && fgetc(f) == 'a');

    // Read, then write with no seek between: the write lands where the reading stopped (1),
    // not where the fd's read-ahead left it (3)
    CHECK(fputs("XY", f) >= 0);
    // Write, then read with no seek between: the output is flushed, never read back from the buffer
    CHECK(fgetc(f) == EOF && feof(f));
    rewind(f);
    CHECK(fread(text, 1, sizeof(text), f) == 3 && memcmp(text, "aXY", 3) == 0);
    CHECK(fclose(f) == 0);

    // "a": every write at the end; ftell knows it only after the write
    f = fopen(FILE_A, "a");
    CHECK(f != NULL && fputs("Z", f) >= 0 && ftell(f) == 4 && fclose(f) == 0);

    // "a+": reading starts at the beginning, writing still goes to the end
    f = fopen(FILE_A, "a+");
    CHECK(f != NULL && fgetc(f) == 'a');
    CHECK(fputs("!", f) >= 0);
    rewind(f);
    CHECK(fread(text, 1, sizeof(text), f) == 5 && memcmp(text, "aXYZ!", 5) == 0);
    CHECK(fclose(f) == 0);

    // "r+": overwrite in the middle after a read and a seek, as C asks
    f = fopen(FILE_A, "r+");
    CHECK(f != NULL && fgetc(f) == 'a');
    CHECK(fseek(f, 0, SEEK_CUR) == 0 && fputc('b', f) == 'b' && ftell(f) == 2);
    CHECK(fclose(f) == 0);
    CHECK(strcmp(contents(FILE_A, text, sizeof(text)), "abYZ!") == 0);

    CHECK(unlink(FILE_A) == 0);

    // A pipe has no position: ESPIPE, and the stream keeps what it read ahead
    int fds[2];
    CHECK(pipe(fds) == 0 && write(fds[1], "xyz", 3) == 3);
    f = fdopen(fds[0], "r");
    CHECK(f != NULL && fgetc(f) == 'x');
    errno = 0;
    CHECK(ftell(f) == -1 && errno == ESPIPE);
    errno = 0;
    CHECK(fseek(f, 0, SEEK_SET) == -1 && errno == ESPIPE);
    CHECK(fgetc(f) == 'y' && fgetc(f) == 'z');
    CHECK(fclose(f) == 0 && close(fds[1]) == 0);
}

static int shared_fd = -1;

// Reads "hell" through its own stream on the parent's open file, then exits
static void read_four(void)
{
    FILE *f = fdopen(dup(shared_fd), "r");
    CHECK(f != NULL);
    for (int i = 0; i < 4; i++)
        CHECK(fgetc(f) == "hell"[i]);
}

// What a read stream read ahead goes back to the fd: dup'd fds share one offset, so the other
// fd shows where the stream stopped
static void test_give_back(void)
{
    make_file(FILE_A, "hello\nmore!\n");

    // fclose
    int fd = open(FILE_A, O_RDONLY);
    int other = dup(fd);
    FILE *f = fdopen(fd, "r");
    CHECK(f != NULL && fgetc(f) == 'h' && fgetc(f) == 'e');
    CHECK(lseek(other, 0, SEEK_CUR) == 12); // the buffer took everything
    CHECK(fclose(f) == 0);
    CHECK(lseek(other, 0, SEEK_CUR) == 2);

    // fflush of an input stream, also undoing an ungetc
    CHECK(lseek(other, 0, SEEK_SET) == 0);
    f = fdopen(dup(other), "r");
    CHECK(f != NULL && fgetc(f) == 'h' && fgetc(f) == 'e' && fgetc(f) == 'l');
    CHECK(fflush(f) == 0 && lseek(other, 0, SEEK_CUR) == 3);
    CHECK(fgetc(f) == 'l' && ungetc('Q', f) == 'Q');
    CHECK(fflush(f) == 0 && lseek(other, 0, SEEK_CUR) == 3 && fgetc(f) == 'l');
    CHECK(fclose(f) == 0);

    // exit, in a child sharing the offset through fork
    CHECK(lseek(other, 0, SEEK_SET) == 0);
    shared_fd = other;
    CHECK(in_child(read_four) == 0);
    CHECK(lseek(other, 0, SEEK_CUR) == 4);

    CHECK(close(other) == 0);
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
    test_fread();
    test_seek_and_tell();
    test_read_and_write();
    test_give_back();
    return test_status();
}
