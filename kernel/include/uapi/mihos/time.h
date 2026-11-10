#ifndef _UAPI_MIHOS_TIME_H_
#define _UAPI_MIHOS_TIME_H_

#include <stdint.h>

// clock_gettime's clocks (Linux values)
#define MIHOS_CLOCK_REALTIME 0  // the wall clock: since 1970-01-01 UTC, may be set
#define MIHOS_CLOCK_MONOTONIC 1 // since boot, never jumps

struct mihos_timespec
{
    int64_t tv_sec;  // seconds
    int64_t tv_nsec; // nanoseconds, 0-999 999 999
};

#endif /* _UAPI_MIHOS_TIME_H_ */