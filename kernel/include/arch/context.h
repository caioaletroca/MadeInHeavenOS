#ifndef _ARCH_CONTEXT_H_
#define _ARCH_CONTEXT_H_

#include <stddef.h>

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
 * @brief Enter the scheduler from the current thread, saving its context.
 */
void arch_yield(void);

#endif // _ARCH_CONTEXT_H_
