#include <string.h>
#include <test.h>

/*
 * <string.h> functions whose edge cases are easy to get wrong. memmove is
 * memcpy that allows the two ranges to overlap: moving bytes to the right
 * inside one buffer (inserting) must copy from the end, or the first bytes
 * written overwrite ones not yet read.
 */

static void test_memmove_left(void)
{
    char text[] = "xxabcd";
    CHECK(memmove(text, text + 2, 5) == text);
    CHECK(strcmp(text, "abcd") == 0);
}

// What a line editor does to insert: the rest of the line, '\0' included, one byte right
static void test_memmove_right(void)
{
    char text[16] = "echo ac";
    memmove(text + 7, text + 6, 2);
    text[6] = 'b';
    CHECK(strcmp(text, "echo abc") == 0);

    char wide[] = "abcdef--";
    memmove(wide + 2, wide, 6);
    CHECK(memcmp(wide, "ababcdef", 8) == 0);
}

static void test_memmove_same_and_empty(void)
{
    char text[] = "same";
    memmove(text, text, 4);
    CHECK(strcmp(text, "same") == 0);

    memmove(text + 1, text, 0);
    CHECK(strcmp(text, "same") == 0);
}

int main(void)
{
    test_memmove_left();
    test_memmove_right();
    test_memmove_same_and_empty();
    return test_status();
}
