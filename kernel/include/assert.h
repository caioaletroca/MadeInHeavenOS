#ifndef _ASSERT_H_
#define _ASSERT_H_

#include <panic.h>

/**
 * Asserts that the given condition is true. If the condition is false, the kernel will panic with an error message.
 */
#define KASSERT(condition)                                                                     \
    do                                                                                         \
    {                                                                                          \
        if (!(condition))                                                                      \
        {                                                                                      \
            panic("Assertion failed: %s, file %s, line %d\n", #condition, __FILE__, __LINE__); \
        }                                                                                      \
    } while (0)

#endif