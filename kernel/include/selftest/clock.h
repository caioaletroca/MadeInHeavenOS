#ifndef _SELFTEST_CLOCK_H_
#define _SELFTEST_CLOCK_H_

/**
 * Self-test for the clocks: clock_days_from_civil against known dates (the
 * epoch, the day before it, leap days, a century that is not a leap year),
 * the wall clock read from the RTC being a plausible date, and the monotonic
 * clock advancing by at least the ticks slept. Panics on failure.
 */
void clock_selftest(void);

#endif // _SELFTEST_CLOCK_H_
