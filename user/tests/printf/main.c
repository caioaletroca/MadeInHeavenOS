#include <stdarg.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <test.h>

/*
 * vsnprintf (shared with the kernel through libk.a): every conversion, flag
 * and length it supports, then the C99 contract: the output never overruns,
 * is terminated when size > 0, and the return value is the full length.
 */

// Formats into a large buffer; the result and the returned length must match
static void expect(int line, const char *expected, const char *format, ...)
{
    char buffer[128];
    va_list args;

    va_start(args, format);
    int length = vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    if (strcmp(buffer, expected) != 0 || length != (int)strlen(expected))
    {
        test_failures++;
        printf("FAIL line %d: \"%s\" gave \"%s\" (%d), expected \"%s\"\n", line, format, buffer, length, expected);
    }
}

#define EXPECT(expected, ...) expect(__LINE__, expected, __VA_ARGS__)

static void test_integers(void)
{
    EXPECT("0", "%d", 0);
    EXPECT("-42", "%i", -42);
    EXPECT("2147483647", "%d", 2147483647);
    EXPECT("-2147483648", "%d", -2147483647 - 1);
    EXPECT("4294967295", "%u", 4294967295u);

    // Above 2^32: the l/ll/z arguments must be read as 64 bits
    EXPECT("4294967296", "%ld", 4294967296L);
    EXPECT("-4294967296", "%ld", -4294967296L);
    EXPECT("-9223372036854775808", "%lld", INT64_MIN); // its magnitude does not fit in a long long
    EXPECT("18446744073709551615", "%llu", UINT64_MAX);
    EXPECT("18446744073709551615", "%zu", (size_t)-1);
    EXPECT("-5", "%zd", (ssize_t)-5);

    EXPECT("123456789abcdef", "%lx", 0x123456789abcdefUL);
    EXPECT("ABCDEF", "%X", 0xABCDEFu);
    EXPECT("ffffffff", "%x", -1);
    EXPECT("10", "%o", 8);
    EXPECT("1777777777777777777777", "%llo", UINT64_MAX); // 22 digits, the longest number

    // hh and h: the argument arrives as int, but is printed as char / short (C11 7.21.6.1)
    EXPECT("44", "%hhd", 300);
    EXPECT("-56", "%hhd", 200);
    EXPECT("0", "%hhu", 256);
    EXPECT("1", "%hd", 65537);
    EXPECT("ffff", "%hx", -1);
}

static void test_flags(void)
{
    EXPECT("   42", "%5d", 42);
    EXPECT("42   |", "%-5d|", 42);
    EXPECT("00042", "%05d", 42);
    EXPECT("-0042", "%05d", -42); // zeros after the sign
    EXPECT("42   |", "%-05d|", 42); // '-' wins over '0'
    EXPECT("+42", "%+d", 42);
    EXPECT("-42", "%+d", -42);
    EXPECT(" 42", "% d", 42);
    EXPECT("+42", "%+ d", 42); // '+' wins over ' '
    EXPECT("  +42", "%+5d", 42);
    EXPECT("5", "%+u", 5u); // unsigned conversions have no sign
    EXPECT("42", "%1d", 42); // a width smaller than the number cuts nothing
}

static void test_precision(void)
{
    EXPECT("007", "%.3d", 7);
    EXPECT("-007", "%.3d", -7);
    EXPECT("00a", "%.3x", 0xa);
    EXPECT("", "%.0d", 0); // precision 0 and value 0: no digits
    EXPECT("", "%.d", 0);  // '.' alone is precision 0
    EXPECT("     |", "%5.0d|", 0);
    EXPECT("    -007", "%8.3d", -7);
    EXPECT("     007", "%08.3d", 7); // '0' is ignored with a precision
    EXPECT("1", "%.0d", 1);
}

static void test_star(void)
{
    EXPECT("   42", "%*d", 5, 42);
    EXPECT("42   |", "%*d|", -5, 42); // a negative width is '-' plus the width
    EXPECT("007", "%.*d", 3, 7);
    EXPECT("0", "%.*d", -1, 0); // a negative precision is no precision
    EXPECT("ab    |", "%-*.*s|", 6, 2, "abc");
    EXPECT("  x 9", "%*c %d", 3, 'x', 9); // '*' takes its argument in order
}

static void test_strings(void)
{
    // Not terminated: the precision must stop the read, not just the output
    const char unterminated[3] = {'a', 'b', 'c'};

    EXPECT("hi", "%s", "hi");
    EXPECT("   hi", "%5s", "hi");
    EXPECT("hi   |", "%-5s|", "hi");
    EXPECT("h", "%.1s", "hi");
    EXPECT("hi", "%.9s", "hi");
    EXPECT("", "%.0s", "hi");
    EXPECT("abc", "%.3s", unterminated);
    EXPECT("(null)", "%s", (char *)NULL);
    EXPECT("", "%s", "");

    EXPECT("A", "%c", 'A');
    EXPECT("  A", "%3c", 'A');
    EXPECT("A  |", "%-3c|", 'A');
    EXPECT("\xE9", "%c", 0xE9); // a byte above 127 passes unchanged

    EXPECT("100%", "100%%");
    EXPECT("key=v, n=-3, ok", "%s=%c, n=%d, %s", "key", 'v', -3, "ok");
}

static void test_pointer(void)
{
    EXPECT("0000000000001234", "%p", (void *)0x1234);
    EXPECT("0000000000000000", "%p", NULL);
    EXPECT("ffff800000000000", "%p", (void *)0xffff800000000000UL);
}

static void test_unknown(void)
{
    // Printed as written, without taking an argument
    EXPECT("%q", "%q");
    EXPECT("%5q", "%5q");
    EXPECT("%q 5", "%q %d", 5);
    EXPECT("abc%", "abc%");
    EXPECT("abc%-5", "abc%-5");
}

static void test_cut(void)
{
    char buffer[8];

    // Cut: terminated inside size, nothing written after it, full length returned
    memset(buffer, '#', sizeof(buffer));
    CHECK(snprintf(buffer, 4, "abcdef") == 6);
    CHECK(memcmp(buffer, "abc\0####", 8) == 0);

    // Exactly fits: size == length + 1
    memset(buffer, '#', sizeof(buffer));
    CHECK(snprintf(buffer, 4, "abc") == 3);
    CHECK(memcmp(buffer, "abc\0####", 8) == 0);

    // One short: the last character gives way to the terminator
    memset(buffer, '#', sizeof(buffer));
    CHECK(snprintf(buffer, 3, "abc") == 3);
    CHECK(memcmp(buffer, "ab\0#####", 8) == 0);

    // Cut inside padding and inside a number
    memset(buffer, '#', sizeof(buffer));
    CHECK(snprintf(buffer, 4, "%10d", 5) == 10);
    CHECK(memcmp(buffer, "   \0####", 8) == 0);
    memset(buffer, '#', sizeof(buffer));
    CHECK(snprintf(buffer, 4, "%d", -123456) == 7);
    CHECK(memcmp(buffer, "-12\0####", 8) == 0);

    // size 1: only the terminator
    memset(buffer, '#', sizeof(buffer));
    CHECK(snprintf(buffer, 1, "abc") == 3);
    CHECK(memcmp(buffer, "\0#######", 8) == 0);

    // size 0: nothing written, str may be NULL; the length still counts
    memset(buffer, '#', sizeof(buffer));
    CHECK(snprintf(buffer, 0, "abc") == 3);
    CHECK(memcmp(buffer, "########", 8) == 0);
    CHECK(snprintf(NULL, 0, "%d items", 12345) == 11);

    // Empty output is still terminated
    memset(buffer, '#', sizeof(buffer));
    CHECK(snprintf(buffer, sizeof(buffer), "%s", "") == 0);
    CHECK(buffer[0] == '\0' && buffer[1] == '#');

    // Longer than any buffer: the length is counted past the end
    char line[16];
    CHECK(snprintf(line, sizeof(line), "%300d", 1) == 300);
    CHECK(strlen(line) == sizeof(line) - 1);
}

int main(void)
{
    test_integers();
    test_flags();
    test_precision();
    test_star();
    test_strings();
    test_pointer();
    test_unknown();
    test_cut();
    return test_status();
}
