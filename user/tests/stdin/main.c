#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <test.h>

/*
 * stdio input without typing: pushback, reading a write-only stream, and a
 * closed stdin. The parts that need a keyboard are in stdin_interactive.
 */

#define WRITE_ONLY "/mnt/stdin-write-only" // the ext2 disk: writable in both boots

static void test_ungetc(void)
{
    char line[8];

    // A pushed-back byte comes back without reading (this would block otherwise)
    CHECK(ungetc('x', stdin) == 'x');
    CHECK(getchar() == 'x');

    // Stored as unsigned char: a negative char comes back as 0..255, like fgetc returns it
    char accented = (char)0xE9;
    CHECK(ungetc(accented, stdin) == 0xE9);
    CHECK(getchar() == 0xE9);

    CHECK(ungetc(EOF, stdin) == EOF);

    // fgets takes the pushed-back byte and stops when the array is full
    CHECK(ungetc('a', stdin) == 'a');
    CHECK(fgets(line, 2, stdin) == line);
    CHECK(line[0] == 'a' && line[1] == '\0');

    // n == 1: no room for a character, nothing is read, the result is ""
    line[0] = 'z';
    CHECK(fgets(line, 1, stdin) == line);
    CHECK(line[0] == '\0');
}

/*
 * A stream on an fd open only for writing: the read fails (EBADF), so EOF and
 * the error indicator, not a byte from the buffer. Not stdout:
 * which libcs refuse it by the stream's mode is their choice (glibc and ours
 * do; mlibc reads fd 1, the console here, and waits for a key).
 */
static void test_write_only(void)
{
    FILE *f = fopen(WRITE_ONLY, "w");
    CHECK(f != NULL);
    if (f == NULL)
        return;
    CHECK(fputs("data", f) >= 0);

    // errno is EBADF on mlibc (from read); ours refuses earlier and leaves it
    CHECK(fgetc(f) == EOF);
    CHECK(ferror(f));
    CHECK(!feof(f));
    clearerr(f);
    CHECK(!ferror(f));

    CHECK(fclose(f) == 0);
    CHECK(unlink(WRITE_ONLY) == 0);
}

static void test_closed(void)
{
    char line[8];

    CHECK(close(0) == 0);

    errno = 0;
    CHECK(getchar() == EOF);
    CHECK(errno == EBADF);
    CHECK(ferror(stdin));
    CHECK(!feof(stdin)); // an error, not end of file

    CHECK(fgets(line, sizeof(line), stdin) == NULL);

    clearerr(stdin);
    CHECK(!ferror(stdin));
}

int main(void)
{
    test_ungetc();
    test_write_only();
    test_closed();
    return test_status();
}
