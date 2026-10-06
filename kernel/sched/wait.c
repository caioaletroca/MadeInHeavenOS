#include <sched/wait.h>
#include <sched/scheduler.h>
#include <asm/irq_flags.h>

void wait_queue_init(wait_queue_t *queue)
{
    list_init(&queue->threads);
}

void wait_queue_sleep_locked(wait_queue_t *queue, spinlock_t *lock)
{
    spinlock_release(lock);
    scheduler_block(&queue->threads);
    spinlock_acquire(lock);
}

bool wait_queue_wake_one(wait_queue_t *queue)
{
    irq_flags_t flags = irq_save();
    bool woken = false;

    if (!list_empty(&queue->threads))
    {
        scheduler_wake(list_container(queue->threads.next, thread_t, run_link));
        woken = true;
    }

    irq_restore(flags);
    return woken;
}

size_t wait_queue_wake_all(wait_queue_t *queue)
{
    irq_flags_t flags = irq_save();
    size_t count = 0;

    // scheduler_wake() unlinks the thread, so the queue shrinks every iteration
    while (!list_empty(&queue->threads))
    {
        scheduler_wake(list_container(queue->threads.next, thread_t, run_link));
        count++;
    }

    irq_restore(flags);
    return count;
}