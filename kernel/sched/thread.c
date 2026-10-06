#include <sched/thread.h>
#include <sched/scheduler.h>
#include <sched/process.h>
#include <sched/sync.h>
#include <arch/context.h>
#include <asm/irq_flags.h>
#include <mm/kmalloc.h>
#include <panic.h>

static spinlock_t tid_lock = SPINLOCK_INIT;
static uint32_t next_thread_id = 1;

static spinlock_t zombie_lock = SPINLOCK_INIT;
static list_t zombie_list;

static semaphore_t zombie_count;

/**
 * Allocate a unique thread ID.
 *
 * Guarantees that each thread receives a unique ID even with interrupts disabled.
 */
static uint32_t thread_alloc_id(void)
{
    guard(spinlock, &tid_lock);
    return next_thread_id++;
}

/**
 * Reaper thread function.
 *
 * This function runs in a dedicated thread and is responsible for freeing
 * the resources of terminated threads. It waits for threads to appear in
 * the zombie list and then deallocates their stacks and thread structures.
 */
static void thread_reaper(void *arg)
{
    (void)arg;

    for (;;)
    {
        semaphore_down(&zombie_count);

        thread_t *zombie;
        process_t *process;

        scoped_guard(spinlock, &zombie_lock)
        {
            zombie = list_container(zombie_list.next, thread_t, run_link);
            list_delete(&zombie->run_link);
        }

        process = zombie->process;
        kfree(zombie->stack);
        kfree(zombie);
        process_release(process);
    }
}

/**
 * Fill in a thread around an already built context.
 */
static void thread_setup(thread_t *thread, void *stack, size_t stack_size, void *context)
{
    thread->id = thread_alloc_id();
    thread->flags = 0;
    thread->stack = stack;
    thread->stack_size = stack_size;
    thread->entry = NULL;
    thread->arg = NULL;
    thread->state = THREAD_READY;
    thread->context = context;
    thread->wake_tick = 0;
    thread->process = NULL;
    thread->space = NULL;
    list_init(&thread->run_link);
}

void thread_start(void)
{
    thread_t *thread = scheduler_current();
    thread->entry(thread->arg);
    thread_exit();
}

void threads_init(void)
{
    list_init(&zombie_list);
    semaphore_init(&zombie_count, 0);

    if (thread_create(thread_reaper, NULL) == NULL)
        panic("threads_init: failed to create thread reaper");
}

int thread_init(thread_t *thread, void *stack, size_t stack_size, thread_entry_t entry, void *arg)
{
    if (thread == NULL || stack == NULL || entry == NULL)
        return -1;

    void *context = arch_thread_context_init(stack, stack_size);
    if (context == NULL)
        return -1;

    thread_setup(thread, stack, stack_size, context);
    thread->entry = entry;
    thread->arg = arg;

    return 0;
}

thread_t *thread_create(thread_entry_t entry, void *arg)
{
    thread_t *thread = kzalloc(sizeof(thread_t));
    void *stack = kmalloc(THREAD_STACK_SIZE);

    if (thread == NULL || stack == NULL || thread_init(thread, stack, THREAD_STACK_SIZE, entry, arg) != 0)
    {
        kfree(stack);
        kfree(thread);
        return NULL;
    }

    thread->flags |= THREAD_FLAG_OWNED;

    if (scheduler_add(thread) != 0)
    {
        panic("thread_create: new thread rejected by the scheduler");
    }

    return thread;
}

thread_t *thread_create_user(process_t *process, uintptr_t entry, uintptr_t user_stack)
{
    thread_t *thread = kzalloc(sizeof(thread_t));
    void *stack = kmalloc(THREAD_STACK_SIZE);
    void *context = NULL;

    if (process != NULL && thread != NULL && stack != NULL)
        context = arch_user_context_init(stack, THREAD_STACK_SIZE, entry, user_stack);

    if (context == NULL)
    {
        kfree(stack);
        kfree(thread);
        return NULL;
    }

    thread_setup(thread, stack, THREAD_STACK_SIZE, context);
    thread->flags |= THREAD_FLAG_OWNED;
    thread->process = process;
    thread->space = process->space;

    if (scheduler_add(thread) != 0)
        panic("thread_create_user: new thread rejected by the scheduler\n");

    return thread;
}

void thread_sleep(uint64_t ticks)
{
    scheduler_sleep(ticks);
}

__attribute__((noreturn)) void thread_exit(void)
{
    // Never restored: this thread does not run again, and with IF=0 the
    // reaper cannot free our stack before we have switched away from it.
    (void)irq_save();

    thread_t *self = scheduler_current();
    self->state = THREAD_TERMINATED;

    if (self->flags & THREAD_FLAG_OWNED)
    {
        spinlock_acquire(&zombie_lock);
        list_insert_before(&zombie_list, &self->run_link);
        semaphore_up(&zombie_count);
        spinlock_release(&zombie_lock);
    }

    scheduler_yield();

    panic("Terminated thread was resumed\n");
}
