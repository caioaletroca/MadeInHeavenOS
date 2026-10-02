#ifndef _SCHEDULER_H_
#define _SCHEDULER_H_

#include <sched/thread.h>
#include <sys/list.h>

/**
 * @brief Trigger a scheduler tick, indicating that the current thread's time slice has expired.
 */
void scheduler_tick(void);

/**
 * @brief Initialize the scheduler with the boot thread.
 *
 * @param boot_thread Pointer to the boot thread to initialize the scheduler with.
 * @note This function should be called once during system initialization with the boot thread.
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
 * @brief Block the current thread, removing it from the scheduler's run queue.
 *
 * @param queue Pointer to the list representing the queue to block the current thread on.
 */
void scheduler_block(list_t *queue);

/**
 * @brief Wake the specified thread, adding it back to the scheduler's run queue.
 *
 * @param thread Pointer to the thread to wake.
 */
void scheduler_wake(thread_t *thread);

/**
 * @brief Block the current thread for at least `ticks` scheduler ticks.
 *
 * @param ticks Number of scheduler ticks to sleep.
 *
 * @note The actual sleep duration may be longer than the specified number of ticks due to scheduling delays.
 */
void scheduler_sleep(uint64_t ticks);

/**
 * @brief Ask for a context switch at the next interrupt exit.
 */
void scheduler_request_reschedule(void);

/**
 * @brief Check whether a context switch was requested.
 *
 * @return Non-zero if a reschedule is pending.
 */
int scheduler_need_reschedule(void);

/**
 * @brief Switch threads on interrupt exit.
 *
 * @param context Saved context of the interrupted thread (opaque, arch-defined).
 *
 * @return Saved context of the thread to resume.
 */
void *scheduler_on_interrupt(void *context);

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
