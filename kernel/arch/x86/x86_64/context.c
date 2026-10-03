#include <string.h>
#include <util.h>
#include <arch/context.h>
#include <x86/cpu.h>
#include <x86/gdt.h>
#include <x86/isr.h>
#include <x86/vectors.h>
#include <x86/tss.h>

extern void thread_entry(void);

void *arch_thread_context_init(void *stack, size_t stack_size)
{
    if (stack == NULL || stack_size < sizeof(isr_context_t) + ABI_STACK_ALIGNMENT)
        return NULL;

    // thread_entry is reached via iretq (not call), so RSP must be 16-byte
    // aligned there; its `call thread_start` then satisfies the SysV ABI.
    uintptr_t stack_top = ALIGN_DOWN((uintptr_t)stack + stack_size, ABI_STACK_ALIGNMENT);

    // The frame lives just below the top; iretq consumes it before the
    // thread starts using the stack, so the overlap is harmless.
    isr_context_t *context = (isr_context_t *)(stack_top - sizeof(isr_context_t));
    memset(context, 0, sizeof(*context));

    context->rip = (uintptr_t)thread_entry;
    context->cs = KERNEL_CODE_SELECTOR;
    context->rflags = RFLAGS_KERNEL_THREAD;
    context->rsp = stack_top;
    context->ss = KERNEL_DATA_SELECTOR;

    return context;
}

void *arch_user_context_init(void *stack, size_t stack_size, uintptr_t entry, uintptr_t user_stack)
{
    // Same placement and zeroing as a kernel thread; only the frame differs
    isr_context_t *context = arch_thread_context_init(stack, stack_size);

    if (context == NULL)
        return NULL;

    // iretq with RPL 3 selectors drops to ring 3 at entry
    context->rip = entry;
    context->cs = USER_CODE_SELECTOR;
    context->rflags = RFLAGS_USER_THREAD;
    context->rsp = user_stack;
    context->ss = USER_DATA_SELECTOR;

    return context;
}

void arch_thread_switch(void *stack, size_t stack_size)
{
    // The CPU loads RSP0 when an interrupt arrives in ring 3. Kernel
    // threads never run there, so only user threads need it.
    if (stack != NULL)
        tss_set_kernel_stack(ALIGN_DOWN((uintptr_t)stack + stack_size, ABI_STACK_ALIGNMENT));
}

void arch_yield(void)
{
    // Goes through isr_common, so the saved frame has the same layout as a
    // preempted thread. `int` ignores IF, so this works with IRQs disabled.
    __asm__ __volatile__("int %0" : : "i"(VECTOR_SCHEDULER_YIELD) : "memory");
}
