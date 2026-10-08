#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <test.h>

/*
 * stdio input without typing: pushback, reading a write-only stream, and a
 * closed stdin. The parts that need a keyboard are in stdin_interactive.
 */

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

static void test_write_only(void)
{
    // stdout is not readable: EOF and the error indicator, not a byte from its buffer
    CHECK(fgetc(stdout) == EOF);
    CHECK(ferror(stdout));
    CHECK(!feof(stdout));
    clearerr(stdout);
    CHECK(!ferror(stdout));
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
