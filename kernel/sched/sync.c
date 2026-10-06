#include <sched/sync.h>
#include <sched/scheduler.h>
#include <assert.h>

void semaphore_init(semaphore_t *semaphore, unsigned int value)
{
    semaphore->count = value;
    wait_queue_init(&semaphore->waiters);
    spinlock_init(&semaphore->lock);
}

void semaphore_down(semaphore_t *semaphore)
{
    scoped_guard(spinlock, &semaphore->lock)
    {
        while (semaphore->count == 0)
            wait_queue_sleep_locked(&semaphore->waiters, &semaphore->lock);

        semaphore->count--;
    }
}

void semaphore_up(semaphore_t *semaphore)
{
    scoped_guard(spinlock, &semaphore->lock)
    {
        semaphore->count++;
        wait_queue_wake_one(&semaphore->waiters);
    }
}

void mutex_init(mutex_t *mutex)
{
    mutex->owner = NULL;
    wait_queue_init(&mutex->waiters);
    spinlock_init(&mutex->lock);
}

void mutex_lock(mutex_t *mutex)
{
    scoped_guard(spinlock, &mutex->lock)
    {
        thread_t *self = scheduler_current();

        KASSERT(mutex->owner != self);

        while (mutex->owner != NULL)
            wait_queue_sleep_locked(&mutex->waiters, &mutex->lock);

        mutex->owner = self;
    }
}

void mutex_unlock(mutex_t *mutex)
{
    scoped_guard(spinlock, &mutex->lock)
    {
        KASSERT(mutex->owner == scheduler_current());

        mutex->owner = NULL;
        wait_queue_wake_one(&mutex->waiters);
    }
}