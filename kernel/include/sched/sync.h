#ifndef _SCHED_SYNC_H_
#define _SCHED_SYNC_H_

#include <sched/thread.h>
#include <sched/wait.h>

typedef struct semaphore
{
    unsigned int count;
    wait_queue_t waiters;
} semaphore_t;

typedef struct mutex
{
    thread_t *owner;
    wait_queue_t waiters;
} mutex_t;

/**
 * Initializes a semaphore with the given value.
 *
 * @param semaphore The semaphore to initialize.
 * @param value The initial value of the semaphore.
 */
void semaphore_init(semaphore_t *semaphore, unsigned int value);

/**
 * Decrements the semaphore count, blocking if the count is zero.
 *
 * @param semaphore The semaphore to decrement.
 */
void semaphore_down(semaphore_t *semaphore);

/**
 * Increments the semaphore count, waking up a waiting thread if any.
 *
 * @param semaphore The semaphore to increment.
 */
void semaphore_up(semaphore_t *semaphore);

/**
 * Initializes a mutex.
 *
 * @param mutex The mutex to initialize.
 */
void mutex_init(mutex_t *mutex);

/**
 * Locks the mutex, blocking if it is already locked.
 *
 * @param mutex The mutex to lock.
 */
void mutex_lock(mutex_t *mutex);

/**
 * Unlocks the mutex.
 *
 * @param mutex The mutex to unlock.
 */
void mutex_unlock(mutex_t *mutex);

#endif /* _SCHED_SYNC_H_ */