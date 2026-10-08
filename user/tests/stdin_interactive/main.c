#include <string.h>
#include <stdio.h>
#include <test.h>

/*
 * stdio input with the keyboard. Only in the "interactive tests" GRUB entry:
 * it waits for typing. Follow the prompts; every prompt has no newline, so
 * seeing it at all checks that line-buffered stdout is flushed before a read.
 */

int main(void)
{
    char line[64];

    // 1. A whole line: the newline is kept
    printf("1. Type hello and press Enter: ");
    CHECK(fgets(line, sizeof(line), stdin) == line);
    CHECK(strcmp(line, "hello\n") == 0);

    // 2. A line longer than the array comes in pieces, nothing lost
    printf("2. Type abcdefghij and press Enter: ");
    char piece[5]; // 4 characters per call
    CHECK(fgets(piece, sizeof(piece), stdin) && strcmp(piece, "abcd") == 0);
    CHECK(fgets(piece, sizeof(piece), stdin) && strcmp(piece, "efgh") == 0);
    CHECK(fgets(piece, sizeof(piece), stdin) && strcmp(piece, "ij\n") == 0);

    // 3. getchar walks the same buffer
    printf("3. Type xy and press Enter: ");
    CHECK(getchar() == 'x');
    CHECK(getchar() == 'y');
    CHECK(getchar() == '\n');

    // 4. Ctrl+D after some characters sends them without a newline;
    // a second Ctrl+D on the empty line is the end of file
    printf("4. Type abc, then Ctrl+D twice: ");
    CHECK(fgets(line, sizeof(line), stdin) == line);
    CHECK(strcmp(line, "abc") == 0);
    CHECK(feof(stdin));

    // 5. End of file is sticky: no new read, so this returns at once
    CHECK(getchar() == EOF);
    CHECK(fgets(line, sizeof(line), stdin) == NULL);

    // 6. clearerr lets the program read again
    clearerr(stdin);
    printf("\n5. Type ok and press Enter: ");
    CHECK(fgets(line, sizeof(line), stdin) == line);
    CHECK(strcmp(line, "ok\n") == 0);

    printf("stdin_interactive: %d failure(s)\n", test_failures);
    return test_status();
}
