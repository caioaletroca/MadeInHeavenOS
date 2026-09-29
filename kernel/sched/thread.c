#include <sched/thread.h>
#include <sched/scheduler.h>

#include "panic.h"

static void thread_start(void)
{
    thread_t *thread = scheduler_current();
    thread->entry();
    thread_exit();
}

int thread_init(thread_t *thread, void *stack, size_t stack_size, thread_entry_t entry)
{
    if (thread == NULL || stack == NULL || entry == NULL)
        return -1;

    if (stack_size < sizeof(uintptr_t) * 8)
        return -1;

    thread->id = 0;
    thread->stack = stack;
    thread->stack_size = stack_size;
    thread->entry = entry;
    thread->state = THREAD_READY;

    // Align the stack pointer to a 16-byte boundary for ABI compliance.
    uintptr_t *stack_pointer = (uintptr_t *)(((uintptr_t)stack + stack_size) & ~(uintptr_t)0xF);
    *(--stack_pointer) = (uintptr_t)thread_start;
    *(--stack_pointer) = 0; // RBP
    *(--stack_pointer) = 0; // RBX
    *(--stack_pointer) = 0; // R12
    *(--stack_pointer) = 0; // R13
    *(--stack_pointer) = 0; // R14
    *(--stack_pointer) = 0; // R15

    thread->stack_pointer = stack_pointer;
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