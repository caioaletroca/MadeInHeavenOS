#ifndef _SCHEDULER_H_
#define _SCHEDULER_H_

#include <sched/thread.h>
#include <vectors.h>
#include "isr.h"

#define SCHEDULER_YIELD_VECTOR VECTOR_SCHEDULER_YIELD

/**
 * @brief Trigger a scheduler tick, indicating that the current thread's time slice has expired.
 */
void scheduler_tick(void);

/**
 * @brief Initialize the scheduler with the boot thread.
 *
 * @param boot_thread Pointer to the boot thread to initialize the scheduler with.
 * @note This function should be called once during system initialization with the boot thread.
 *
 * @return 0 on success, or a negative error code on failure.
 */
void scheduler_init(thread_t *boot_thread);

/**
 * @brief Add a thread to the scheduler's run queue.
 *
 * @param thread Pointer to the thread to add to the scheduler's run queue.
 *
 * @return 0 on success, or a negative error code on failure.
 */
int scheduler_add(thread_t *thread);

/**
 * @brief Handle scheduler-related tasks during an interrupt.
 *
 * @param context Pointer to the ISR context of the interrupt.
 *
 * @return Pointer to the ISR context after handling the interrupt.
 */
isr_context_t *scheduler_on_interrupt(isr_context_t *context);

/**
 * @brief Yield the CPU to the next thread in the scheduler's run queue.
 */
void scheduler_yield(void);

/**
 * @brief Get the currently running thread.
 *
 * @return Pointer to the currently running thread.
 */
thread_t *scheduler_current(void);

#endif // _SCHEDULER_H_