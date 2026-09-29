#ifndef _TIMER_H_
#define _TIMER_H_

#include <stdint.h>

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

#endif // _TIMER_H_