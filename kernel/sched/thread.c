#include <sched/thread.h>
#include <sched/scheduler.h>
#include <string.h>
#include "isr.h"
#include "panic.h"

extern void thread_entry(void);

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

    if (stack_size < sizeof(isr_context_t) + 16)
        return -1;

    // TODO: Assign a unique thread ID. For now, we just set it to 0.
    thread->id = 0;
    thread->stack = stack;
    thread->stack_size = stack_size;
    thread->entry = entry;
    thread->state = THREAD_READY;

    // Align the stack pointer to a 16-byte boundary for ABI compliance.
    uintptr_t stack_top = ((uintptr_t)stack + stack_size) & ~(uintptr_t)0xF;

    isr_context_t *context = (isr_context_t *)(stack_top - sizeof(isr_context_t));
    memset(context, 0, sizeof(*context));

    context->rip = (uint64_t)thread_entry;
    context->cs = KERNEL_CODE_SELECTOR;
    context->rflags = RFLAGS_INITIAL;
    context->rsp = stack_top;
    context->ss = KERNEL_DATA_SELECTOR;

    thread->stack_pointer = (uintptr_t)context;
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