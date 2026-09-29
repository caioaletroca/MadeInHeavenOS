#ifndef _SCHEDULER_H_
#define _SCHEDULER_H_

#include <sched/thread.h>

/**
 * @brief Perform a context switch between two threads.
 *
 * @param previous_stack_pointer Pointer to the stack pointer of the currently running thread.
 * @param next_stack_pointer Pointer to the stack pointer of the thread to switch to.
 *
 * @note This function does not return to the previous thread until it is switched back to.
 */
void context_switch(uintptr_t *previous_stack_pointer, uintptr_t next_stack_pointer);

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