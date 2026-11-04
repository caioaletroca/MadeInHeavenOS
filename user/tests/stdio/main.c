#include <sys/wait.h>
#include <unistd.h>
#include <spawn.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <test.h>

/*
 * stdio buffering and return values. The body runs twice:
 *
 * In a child whose stdout and stderr are one pipe, read back and compared
 * exactly. A pipe is not a terminal, so stdout is fully buffered: stderr's
 * line comes first and everything else only at exit's flush:
 *   3 stderr first
 *   1 line-buffered
 *   2 no newline... 4
 *   checks: xabcdabcdabcd123
 *   5 flushed by exit
 *
 * Then on the console, where stdout is line-buffered. Only a terminal shows
 * that, so this order is still checked by eye; expected:
 *   1 line-buffered
 *   3 stderr first
 *   2 no newline... 4
 *   checks: xabcdabcdabcd123
 *   5 flushed by exit       (last, only through exit's flush)
 */

#define PIPED_TEXT "3 stderr first\n"           \
                   "1 line-buffered\n"          \
                   "2 no newline... 4\n"        \
                   "checks: xabcdabcdabcd123\n" \
                   "5 flushed by exit"

static int body(void)
{
    // Buffering modes: stdout line-buffered on a terminal, stderr unbuffered
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

    // Left in the buffer: only exit's flush can print it (on the console the kernel's exit line follows on it)
    printf("5 flushed by exit");
    return test_status();
}

// The body in a child writing both streams into one pipe; its output read back to EOF
static void test_piped(char *self)
{
    int fds[2];
    CHECK(pipe(fds) == 0);

    fflush(stdout);
    int saved_out = dup(1);
    int saved_err = dup(2);
    dup2(fds[1], 1);
    dup2(fds[1], 2);

    char *argv[] = {self, "child", NULL};
    pid_t pid = -1;
    int error = posix_spawn(&pid, self, NULL, NULL, argv, environ);

    // CHECK prints to stdout: only once it is back
    int restored = dup2(saved_out, 1) == 1 && dup2(saved_err, 2) == 2;
    close(saved_out);
    close(saved_err);
    close(fds[1]);
    CHECK(restored && error == 0);

    char text[512];
    size_t done = 0;
    ssize_t n;
    while (done < sizeof(text) - 1 && (n = read(fds[0], text + done, sizeof(text) - 1 - done)) > 0)
        done += (size_t)n;
    text[done] = '\0';
    close(fds[0]);

    if (strcmp(text, PIPED_TEXT) != 0)
        printf("  piped: got \"%s\"\n", text);
    CHECK(strcmp(text, PIPED_TEXT) == 0);

    int status;
    CHECK(error == 0 && waitpid(pid, &status, 0) == pid && WIFEXITED(status) && WEXITSTATUS(status) == 0);
}

int main(int argc, char **argv)
{
    if (argc > 1 && strcmp(argv[1], "child") == 0)
        return body();

    test_piped(argv[0]);
    return body();
}
