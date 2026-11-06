#ifndef _ARCH_SIGNAL_H_
#define _ARCH_SIGNAL_H_

#include <stddef.h>
#include <stdint.h>

/*
 * Architecture contract: running a signal handler on the program's stack.
 * Both act on the frame of the trap from ring 3, at the top of the kernel stack.
 */

struct address_space;

/**
 * @brief Saves the program's registers on its stack and makes the return to
 * ring 3 enter the handler.
 *
 * @return 0 on success, -1 if the stack cannot be written.
 */
int arch_signal_frame_push(void *stack, size_t stack_size, struct address_space *space, int signal,
                           uintptr_t handler, uintptr_t restorer, uint32_t blocked);

/**
 * @brief Puts back what arch_signal_frame_push saved. Checks everything: the
 * frame is user memory.
 *
 * @param blocked Receives the mask to restore.
 * @param result Receives the saved rax: the system call's return value overwrites it.
 * @return 0 on success, -1 if the frame is unreadable or invalid.
 */
int arch_signal_frame_pop(void *stack, size_t stack_size, struct address_space *space,
                          uint32_t *blocked, long *result);

#endif // _ARCH_SIGNAL_H_