#include <sched/sync.h>
#include <sched/scheduler.h>
#include <asm/irq_flags.h>
#include <assert.h>

void semaphore_init(semaphore_t *semaphore, unsigned int value)
{
    semaphore->count = value;
    wait_queue_init(&semaphore->waiters);
}

void semaphore_down(semaphore_t *semaphore)
{
    irq_flags_t flags = irq_save();

    while (semaphore->count == 0)
        wait_queue_sleep(&semaphore->waiters);

    semaphore->count--;
    irq_restore(flags);
}

void semaphore_up(semaphore_t *semaphore)
{
    irq_flags_t flags = irq_save();

    semaphore->count++;
    wait_queue_wake_one(&semaphore->waiters);

    irq_restore(flags);
}

void mutex_init(mutex_t *mutex)
{
    mutex->owner = NULL;
    wait_queue_init(&mutex->waiters);
}

void mutex_lock(mutex_t *mutex)
{
    irq_flags_t flags = irq_save();
    thread_t *self = scheduler_current();

    KASSERT(mutex->owner != self);

    while (mutex->owner != NULL)
        wait_queue_sleep(&mutex->waiters);

    mutex->owner = self;

    irq_restore(flags);
}

void mutex_unlock(mutex_t *mutex)
{
    irq_flags_t flags = irq_save();

    KASSERT(mutex->owner == scheduler_current());

    mutex->owner = NULL;
    wait_queue_wake_one(&mutex->waiters);

    irq_restore(flags);
}