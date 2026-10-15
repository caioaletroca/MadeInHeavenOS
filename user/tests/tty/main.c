#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <test.h>

/*
 * isatty, and stdout's buffering decision built on it: line-buffered on a
 * terminal, fully buffered otherwise, decided at stdout's first write.
 *
 * Order matters: nothing may be written to stdout before test_buffering,
 * which replaces fd 1 with a read-only file first. Afterwards stdout is
 * that file, so CHECK's FAIL lines are lost; failures still count in the
 * exit status, and test_buffering reports its own on stderr.
 */

#define FILE_PATH "/usr/tests/hello"

static void test_isatty(void)
{
    // fds 0-2 are the console; success leaves errno alone
    for (int fd = 0; fd <= 2; fd++)
    {
        errno = 0;
        CHECK(isatty(fd) == 1);
        CHECK(errno == 0);
    }

    // A file is valid but not a terminal: ENOTTY
    int fd = open(FILE_PATH, O_RDONLY);
    errno = 0;
    CHECK(isatty(fd) == 0 && errno == ENOTTY);

    // A directory, too
    int dir = open("/usr", O_RDONLY | O_DIRECTORY);
    errno = 0;
    CHECK(isatty(dir) == 0 && errno == ENOTTY);
    close(dir);

    // Not open: EBADF from fstat, not ENOTTY
    close(fd);
    errno = 0;
    CHECK(isatty(fd) == 0 && errno == EBADF);
    errno = 0;
    CHECK(isatty(99) == 0 && errno == EBADF);
    errno = 0;
    CHECK(isatty(-1) == 0 && errno == EBADF);
}

static void test_buffering(void)
{
    // fd 1 becomes a read-only file (the lowest free fd) before stdout's first write
    CHECK(close(1) == 0);
    int fd = open(FILE_PATH, O_RDONLY);
    if (fd != 1)
    {
        fprintf(stderr, "FAIL tty: the file got fd %d, not 1\n", fd);
        test_failures++;
        return;
    }

    // Fully buffered now: a whole line stays in the buffer, so the write to the
    // read-only fd has not happened yet (line buffering would fail right here)
    int written = printf("x\n");
    int early_error = ferror(stdout);

    // The flush is the first write: EBADF, reported through the stream
    errno = 0;
    int flushed = fflush(stdout);
    int flush_errno = errno;
    int late_error = ferror(stdout);

    if (written != 2 || early_error)
    {
        fprintf(stderr, "FAIL tty: printf on a non-terminal stdout wrote at once (returned %d, error %d)\n",
                written, early_error);
        test_failures++;
    }
    if (flushed != EOF || flush_errno != EBADF || !late_error)
    {
        fprintf(stderr, "FAIL tty: fflush returned %d, errno %d, error %d (expected EOF, EBADF, set)\n", flushed,
                flush_errno, late_error);
        test_failures++;
    }
}

int main(void)
{
    test_isatty();
    test_buffering();
    return test_status();
}
