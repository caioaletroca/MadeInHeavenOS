#include <line/line.h>
#include <string.h>
#include <stdio.h>
#include <test.h>

/*
 * The line library's history, on its own: lines are numbered from 1 as they
 * are added, as bash's history prints them, and only the newest
 * LINE_HISTORY_MAX are kept. The keys that browse it are the editing test's.
 */

#define ADDED (LINE_HISTORY_MAX + 8)

static line_history_t history;

static void test_empty(void)
{
    CHECK(history.lines.count == 0 && history.total == 0);
    CHECK(line_history_get(&history, 0) == NULL);
    CHECK(line_history_get(&history, 1) == NULL);
}

static void test_add(void)
{
    CHECK(line_history_add(&history, "first") == 0);
    CHECK(line_history_add(&history, "second") == 0);
    CHECK(history.lines.count == 2 && history.total == 2);

    CHECK(strcmp(line_history_get(&history, 1), "first") == 0);
    CHECK(strcmp(line_history_get(&history, 2), "second") == 0);
    CHECK(line_history_get(&history, 3) == NULL);
    CHECK(line_history_get(&history, -1) == NULL);
}

// A copy is kept: the caller's buffer is reused for the next line
static void test_copy(void)
{
    char buffer[16];
    strlcpy(buffer, "third", sizeof(buffer));
    CHECK(line_history_add(&history, buffer) == 0);
    strlcpy(buffer, "changed", sizeof(buffer));

    CHECK(strcmp(line_history_get(&history, 3), "third") == 0);
}

// Nothing worth recalling: not stored, not numbered
static void test_blank(void)
{
    CHECK(line_history_add(&history, "") == 0);
    CHECK(line_history_add(&history, " \t ") == 0);
    CHECK(history.lines.count == 3 && history.total == 3);
}

// Past LINE_HISTORY_MAX the oldest go, and the numbers go on
static void test_full(void)
{
    char line[16];

    for (int number = history.total + 1; number <= ADDED; number++)
    {
        snprintf(line, sizeof(line), "line %d", number);
        CHECK(line_history_add(&history, line) == 0);
    }

    CHECK(history.lines.count == LINE_HISTORY_MAX && history.total == ADDED);

    int oldest = ADDED - LINE_HISTORY_MAX + 1;
    CHECK(line_history_get(&history, oldest - 1) == NULL);

    snprintf(line, sizeof(line), "line %d", oldest);
    CHECK(strcmp(line_history_get(&history, oldest), line) == 0);

    snprintf(line, sizeof(line), "line %d", ADDED);
    CHECK(strcmp(line_history_get(&history, ADDED), line) == 0);
    CHECK(line_history_get(&history, ADDED + 1) == NULL);
}

int main(void)
{
    test_empty();
    test_add();
    test_copy();
    test_blank();
    test_full();
    return test_status();
}
