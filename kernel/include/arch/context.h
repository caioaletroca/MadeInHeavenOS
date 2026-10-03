#ifndef _ARCH_CONTEXT_H_
#define _ARCH_CONTEXT_H_

#include <stddef.h>
#include <stdint.h>

/*
 * Architecture contract: thread execution contexts.
 *
 * A saved context is opaque to generic code. The scheduler only stores
 * the pointer and hands it back to the architecture to resume.
 */

/**
 * @brief Build the initial saved context of a new kernel thread.
 *
 * When resumed, the thread starts executing thread_start().
 *
 * @param stack Lowest address of the thread stack.
 * @param stack_size Size of the thread stack in bytes.
 *
 * @return The saved context to resume, or NULL if the stack is too small.
 */
void *arch_thread_context_init(void *stack, size_t stack_size);

/**
 * @brief Build the initial saved context of a thread that starts in user mode.
 *
 * @param stack Lowest address of the thread's kernel stack (traps from user mode land there).
 * @param stack_size Size of the kernel stack in bytes.
 * @param entry User address where execution starts.
 * @param user_stack Initial user stack pointer.
 *
 * @return The saved context to resume, or NULL if the stack is too small.
 */
void *arch_user_context_init(void *stack, size_t stack_size, uintptr_t entry, uintptr_t user_stack);

/**
 * @brief Prepare the CPU to run a thread: interrupts from user mode will use
 * the top of this kernel stack.
 *
 * @param stack Lowest address of the thread's kernel stack.
 * @param stack_size Size of the kernel stack in bytes.
 */
void arch_thread_switch(void *stack, size_t stack_size);

/**
 * @brief Enter the scheduler from the current thread, saving its context.
 */
void arch_yield(void);

#endif // _ARCH_CONTEXT_H_
