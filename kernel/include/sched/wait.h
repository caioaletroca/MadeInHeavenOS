#ifndef _SCHED_WAIT_H_
#define _SCHED_WAIT_H_

#include <stdbool.h>
#include <stddef.h>
#include <sys/list.h>

typedef struct wait_queue
{
    list_t threads;
} wait_queue_t;

/**
 * @brief Initialize a wait queue.
 *
 * @param queue Pointer to the wait queue to initialize.
 */
void wait_queue_init(wait_queue_t *queue);

/**
 * @brief Put the current thread to sleep on the specified wait queue.
 *
 * @param queue Pointer to the wait queue to sleep on.
 */
void wait_queue_sleep(wait_queue_t *queue);

/**
 * @brief Wake one thread waiting on the specified wait queue.
 *
 * @param queue Pointer to the wait queue to wake a thread from.
 *
 * @return true if a thread was woken, false if the queue was empty.
 */
bool wait_queue_wake_one(wait_queue_t *queue);

/**
 * @brief Wake all threads waiting on the specified wait queue.
 *
 * @param queue Pointer to the wait queue to wake threads from.
 *
 * @return The number of threads that were woken.
 */
size_t wait_queue_wake_all(wait_queue_t *queue);

#endif // _SCHED_WAIT_H_