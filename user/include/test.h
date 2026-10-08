#ifndef _USER_TEST_H_
#define _USER_TEST_H_

#include <stdio.h>

/*
 * Checks for the programs in user/tests. Each test is one translation unit:
 * main returns test_status(), so the exit status is the number of failed
 * checks (0 = passed). The kernel's test runner compares it with the status
 * the module expects (see grub/grub.cfg).
 */

static int test_failures;

// Reports to stdout, so a test may close stderr
#define CHECK(expr)                                        \
    do                                                     \
    {                                                      \
        if (!(expr))                                       \
        {                                                  \
            test_failures++;                               \
            printf("FAIL line %d: %s\n", __LINE__, #expr); \
        }                                                  \
    } while (0)

static inline int test_status(void)
{
    return test_failures;
}

#endif /* _USER_TEST_H_ */
