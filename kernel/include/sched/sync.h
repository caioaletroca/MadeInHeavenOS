#ifndef _SCHED_SYNC_H_
#define _SCHED_SYNC_H_

#include <sched/thread.h>
#include <sched/wait.h>
#include <sched/spinlock.h>

typedef struct semaphore
{
    unsigned int count;   // Protected by lock
    wait_queue_t waiters; // List is scheduler state (IRQs off); see scheduler.c
    spinlock_t lock;
} semaphore_t;

typedef struct mutex
{
    thread_t *owner;      // Protected by lock
    wait_queue_t waiters; // List is scheduler state (IRQs off); see scheduler.c
    spinlock_t lock;
} mutex_t;

/**
 * @brief Guard state for `guard(mutex, m)` / `scoped_guard(mutex, m)`.
 */
typedef struct
{
    mutex_t *mutex; // a pointer: unlocking a copy would leave the real mutex locked
} mutex_guard_t;

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

/**
 * @brief Lock the mutex (`guard(mutex, m)` acquire). May sleep: never with a spinlock held.
 */
static inline mutex_guard_t mutex_guard_init(mutex_t *mutex)
{
    mutex_lock(mutex);
    return (mutex_guard_t){.mutex = mutex};
}

/**
 * @brief Unlock it at the end of the scope (`guard(mutex, m)` release).
 */
static inline void mutex_guard_exit(mutex_guard_t *guard)
{
    mutex_unlock(guard->mutex);
}

#endif /* _SCHED_SYNC_H_ */