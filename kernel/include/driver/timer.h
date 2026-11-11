#ifndef _TIMER_H_
#define _TIMER_H_

#include <stdint.h>

// System tick frequency in Hz
#define TIMER_FREQUENCY_HZ 100

/**
 * @brief Account one timer tick. Called by the platform timer IRQ.
 */
void timer_tick(void);

/**
 * @brief Initializes the system timer with the specified frequency.
 *
 * @param frequency The frequency at which the system timer should operate.
 * @return void
 */
void timer_init(uint32_t frequency);

/**
 * @brief Retrieves the current number of timer ticks.
 *
 * @return The current number of timer ticks.
 */
uint64_t timer_ticks(void);

/**
 * @brief Converts nanoseconds to the timer ticks a sleep must wait to last at
 * least that long: rounded up, plus one for the tick already in progress.
 *
 * @param nanoseconds The time in nanoseconds.
 * @return The number of ticks, at least 1 (even for 0 ns).
 */
uint64_t timer_ticks_for(uint64_t nanoseconds);

/**
 * @brief Retrieves the current time in nanoseconds since the system started.
 *
 * @return The current time in nanoseconds.
 */
uint64_t timer_nanoseconds(void);

/**
 * @brief Converts a count of timer ticks to nanoseconds, at the measured tick length.
 *
 * @param ticks The number of ticks.
 * @return The nanoseconds they last.
 */
uint64_t timer_ticks_to_nanoseconds(uint64_t ticks);

#endif // _TIMER_H_