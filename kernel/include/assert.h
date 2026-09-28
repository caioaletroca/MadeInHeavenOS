#ifndef _ASSERT_H_
#define _ASSERT_H_

#include <panic.h>

#define KASSERT(condition)                                                                     \
    do                                                                                         \
    {                                                                                          \
        if (!(condition))                                                                      \
        {                                                                                      \
            panic("Assertion failed: %s, file %s, line %d\n", #condition, __FILE__, __LINE__); \
        }                                                                                      \
    } while (0)

#endif