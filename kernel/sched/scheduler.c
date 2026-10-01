#include <sched/scheduler.h>
#include <sys/list.h>
#include <irq_flags.h>
#include <panic.h>

static volatile int scheduler_enabled = 0;
static volatile int need_reschedule = 0;

static list_t ready_queue;
static thread_t *current_thread;

// TODO: Implement Two-level thread scheduling like Linux/xv6.

static void scheduler_yield_handler(isr_context_t *context)
{
    (void)context;
    need_reschedule = 1;
}

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

/**
 * Yield the CPU to the next ready thread.
 *
 * @note This function does not perform the actual context switch; it only updates the scheduler's state.
 */
static thread_t *scheduler_next_thread(void)
{
    // Yield the CPU to the next ready thread.
    thread_t *next_thread = scheduler_next_ready();
    if (next_thread == NULL)
    {
        if (current_thread->state == THREAD_RUNNING)
            return current_thread;

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

    return next_thread;
}

void scheduler_init(thread_t *boot_thread)
{
    if (boot_thread == NULL)
        panic("scheduler_init: boot_thread is NULL");

    list_init(&ready_queue);

    current_thread = boot_thread;
    current_thread->state = THREAD_RUNNING;
    list_init(&current_thread->run_link);

    isr_info_t yield_info = {
        .type = ISR_IRQ,
        .handler = scheduler_yield_handler};
    isr_set_info(SCHEDULER_YIELD_VECTOR, &yield_info);

    scheduler_enabled = 1;
}

int scheduler_add(thread_t *thread)
{
    if (thread == NULL || thread->state != THREAD_READY)
        return -1;

    // Insert the thread into the ready queue.
    uint64_t flags = irq_save();
    list_insert_before(&ready_queue, &thread->run_link);
    irq_restore(flags);
    return 0;
}

isr_context_t *scheduler_on_interrupt(isr_context_t *context)
{
    need_reschedule = 0;

    if (!scheduler_enabled)
        return context;

    // Save the current thread's context.
    current_thread->stack_pointer = (uintptr_t)context;

    // Determine the next thread to run.
    thread_t *next_thread = scheduler_next_thread();

    // Return the context of the next thread to run.
    return (isr_context_t *)next_thread->stack_pointer;
}

void scheduler_tick(void)
{
    if (scheduler_enabled)
        need_reschedule = 1;
}

int scheduler_need_reschedule(void)
{
    return need_reschedule;
}

void scheduler_yield(void)
{
    // Trigger a software interrupt to yield the CPU to the scheduler.
    __asm__ __volatile__("int %0" : : "i"(SCHEDULER_YIELD_VECTOR));
}

thread_t *scheduler_current(void)
{
    return current_thread;
}