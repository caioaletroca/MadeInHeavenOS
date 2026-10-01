#include <sched/thread.h>
#include <sched/scheduler.h>
#include <arch/context.h>
#include <panic.h>

void thread_start(void)
{
    thread_t *thread = scheduler_current();
    thread->entry();
    thread_exit();
}

int thread_init(thread_t *thread, void *stack, size_t stack_size, thread_entry_t entry)
{
    if (thread == NULL || stack == NULL || entry == NULL)
        return -1;

    void *context = arch_thread_context_init(stack, stack_size);
    if (context == NULL)
        return -1;

    // TODO: Assign a unique thread ID. For now, we just set it to 0.
    thread->id = 0;
    thread->stack = stack;
    thread->stack_size = stack_size;
    thread->entry = entry;
    thread->state = THREAD_READY;
    thread->context = context;
    list_init(&thread->run_link);

    return 0;
}

__attribute__((noreturn)) void thread_exit(void)
{
    thread_t *thread = scheduler_current();

    thread->state = THREAD_TERMINATED;
    scheduler_yield();

    panic("Terminated thread was resumed\n");
}
