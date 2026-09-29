#ifndef _SELFTEST_SCHEDULER_H
#define _SELFTEST_SCHEDULER_H

#include <sched/scheduler.h>

/**
 * Self-test for the scheduler.
 */
__attribute__((noreturn)) void scheduler_selftest(void);

#endif // _SELFTEST_SCHEDULER_H