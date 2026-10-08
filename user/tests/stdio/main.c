#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <test.h>

/*
 * stdio buffering and return values. The order is checked by eye; expected:
 *   1 line-buffered
 *   3 stderr first
 *   2 no newline... 4
 *   checks: xabcdabcdabcd123
 *   5 flushed by exit       (last, only through exit's flush)
 */
int main(void)
{
    // Buffering modes: stdout line-buffered, stderr unbuffered
    printf("1 line-buffered\n");
    printf("2 no newline... ");
    fprintf(stderr, "3 stderr first\n");
    printf("4\n");

    // Return values
    printf("checks: ");
    CHECK(fputc('x', stdout) == 'x');
    CHECK(fwrite("abcdabcdabcd", 4, 3, stdout) == 3);
    CHECK(printf("%d", 123) == 3);
    CHECK(fputs("", stdout) == 0);
    CHECK(fputs("\n", stdout) == 0);

    // A write error sets the stream's error indicator and errno
    CHECK(!ferror(stderr));
    CHECK(close(2) == 0);
    errno = 0;
    CHECK(fprintf(stderr, "never shown\n") < 0);
    CHECK(ferror(stderr));
    CHECK(errno == EBADF);

    // Left in the buffer: only exit's flush can print it (the kernel's exit line follows on it)
    printf("5 flushed by exit");
    return test_status();
}
