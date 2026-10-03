#include <sched/scheduler.h>
#include <mm/address_space.h>
#include <sys/list.h>
#include <arch/context.h>
#include <asm/irq_flags.h>
#include <asm/cpu.h>
#include <assert.h>
#include <panic.h>

#define IDLE_STACK_SIZE (4 * 1024)

static volatile int scheduler_enabled = 0;
static volatile int need_reschedule = 0;

static list_t ready_queue;
static list_t sleep_queue;
static thread_t *current_thread;

static thread_t idle_thread;
static uint8_t idle_stack[IDLE_STACK_SIZE] __attribute__((aligned(16)));

static volatile uint64_t scheduler_ticks;

// TODO: Implement Two-level thread scheduling like Linux/xv6.

/**
 * Entry point for the idle thread.
 *
 * @param arg Unused argument.
 */
static void idle_thread_entry(void *arg)
{
    (void)arg;

    for (;;)
        cpu_idle();
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
    thread_t *previous_thread = current_thread;
    thread_t *next_thread = scheduler_next_ready();

    if (next_thread == NULL)
    {
        // No ready threads available, check if the current thread can continue running.
        if (previous_thread->state == THREAD_RUNNING)
            return previous_thread;

        // If the current thread is not running, we must switch to the idle thread.
        next_thread = &idle_thread;
    }
    else
    {
        // Remove the next thread from the ready queue before switching to it.
        list_delete(&next_thread->run_link);
    }

    // Save the current thread's state and prepare for context switch.
    if (previous_thread->state == THREAD_RUNNING)
    {
        previous_thread->state = THREAD_READY;

        // Insert the previous thread back into the ready queue if it's not the idle thread.
        if (previous_thread != &idle_thread)
            list_insert_before(&ready_queue, &previous_thread->run_link);
    }
    // At this point, the previous thread's state has been saved and it has been re-queued if necessary.

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
    list_init(&sleep_queue);

    if (thread_init(&idle_thread, idle_stack, sizeof(idle_stack), idle_thread_entry, NULL) != 0)
        panic("scheduler_init: failed to initialize idle thread");

    current_thread = boot_thread;
    current_thread->state = THREAD_RUNNING;
    list_init(&current_thread->run_link);

    scheduler_enabled = 1;
}

int scheduler_add(thread_t *thread)
{
    if (thread == NULL || thread->state != THREAD_READY)
        return -1;

    // Insert the thread into the ready queue.
    irq_flags_t flags = irq_save();
    list_insert_before(&ready_queue, &thread->run_link);
    irq_restore(flags);
    return 0;
}

void *scheduler_on_interrupt(void *context)
{
    need_reschedule = 0;

    if (!scheduler_enabled)
        return context;

    // Save the current thread's context.
    current_thread->context = context;

    // Determine the next thread to run.
    thread_t *next_thread = scheduler_next_thread();

    // Kernel stack for traps from ring 3, then the thread's address space
    // (kernel threads run on the kernel root). Both are cheap when unchanged.
    if (next_thread->space != NULL)
        arch_thread_switch(next_thread->stack, next_thread->stack_size);

    address_space_activate(next_thread->space);

    // Return the context of the next thread to run.
    return next_thread->context;
}

void scheduler_block(list_t *queue)
{
    KASSERT(!irq_enabled());
    KASSERT(current_thread != &idle_thread);

    current_thread->state = THREAD_BLOCKED;
    list_insert_before(queue, &current_thread->run_link);

    // `int` works with IF=0; the saved frame keeps IF=0, so this returns
    // (after a wake-up) still inside the caller's critical section.
    arch_yield();
}

void scheduler_wake(thread_t *thread)
{
    KASSERT(!irq_enabled());

    if (thread->state != THREAD_BLOCKED)
        return;

    list_delete(&thread->run_link);
    thread->state = THREAD_READY;
    list_insert_before(&ready_queue, &thread->run_link);

    // Woken from an IRQ while idling: switch right at interrupt exit
    if (current_thread == &idle_thread)
        scheduler_request_reschedule();
}

void scheduler_sleep(uint64_t ticks)
{
    if (ticks == 0)
    {
        scheduler_yield();
        return;
    }

    irq_flags_t flags = irq_save();

    current_thread->wake_tick = scheduler_ticks + ticks;
    scheduler_block(&sleep_queue);

    irq_restore(flags);
}

void scheduler_request_reschedule(void)
{
    need_reschedule = 1;
}

void scheduler_tick(void)
{
    if (!scheduler_enabled)
        return;

    scheduler_ticks++;

    list_t *node, *next;
    // Wake up any threads whose sleep time has expired.
    // IRQ context, IF=0. O(sleepers) per tick; fine until there are many.
    list_for_each_safe(node, next, &sleep_queue)
    {
        thread_t *thread = list_container(node, thread_t, run_link);

        if (thread->wake_tick <= scheduler_ticks)
            scheduler_wake(thread);
    }

    scheduler_request_reschedule();
}

int scheduler_need_reschedule(void)
{
    return need_reschedule;
}

void scheduler_yield(void)
{
    arch_yield();
}

thread_t *scheduler_current(void)
{
    return current_thread;
}