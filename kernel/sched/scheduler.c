#include <sched/scheduler.h>
#include <sys/list.h>

static list_t ready_queue;
static thread_t *current_thread;

/**
 * Get the next ready thread from the ready queue.
 *
 * @return Pointer to the next ready thread, or NULL if the ready queue is empty.
 */
static thread_t *scheduler_next_ready(void)
{
    if (list_empty(&ready_queue))
        return NULL;

    return list_container(ready_queue.next, thread_t, run_link);
}

void scheduler_init(thread_t *boot_thread)
{
    if (boot_thread == NULL)
        return;

    list_init(&ready_queue);

    current_thread = boot_thread;
    current_thread->state = THREAD_RUNNING;

    list_init(&current_thread->run_link);
}

int scheduler_add(thread_t *thread)
{
    if (thread == NULL || thread->state != THREAD_READY)
        return -1;

    // Insert the thread into the ready queue.
    list_insert_before(&ready_queue, &thread->run_link);
    return 0;
}

void scheduler_yield(void)
{
    // Yield the CPU to the next ready thread.
    thread_t *next_thread = scheduler_next_ready();
    if (next_thread == NULL)
    {
        if (current_thread->state == THREAD_RUNNING)
            return;

        panic("No ready threads available and current thread is not running");
    }

    // Remove the next thread from the ready queue before switching to it.
    list_delete(&next_thread->run_link);

    thread_t *previous_thread = current_thread;

    // Save the current thread's state and prepare for context switch.
    if (previous_thread->state == THREAD_RUNNING)
    {
        previous_thread->state = THREAD_READY;
        list_insert_before(&ready_queue, &previous_thread->run_link);
    }
    else if (previous_thread->state != THREAD_TERMINATED && previous_thread->state != THREAD_READY)
    {
        panic("Invalid thread state during yield");
    }

    // Switch to the next thread.
    next_thread->state = THREAD_RUNNING;
    current_thread = next_thread;

    // Perform the context switch to the next thread.
    context_switch(&previous_thread->stack_pointer, next_thread->stack_pointer);
}

thread_t *scheduler_current(void)
{
    return current_thread;
}