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
 * @param stack The user stack pointer where the signal frame will be pushed.
 * @param stack_size The size of the user stack.
 * @param space The address space of the process.
 * @param signal The signal number to handle.
 * @param handler The address of the signal handler.
 * @param restorer The address of the signal restorer function.
 * @param blocked The signal mask to block during the handler.
 * @param restart_number The system call to run again if this return is one ending
 *        with EINTR, or SYSCALL_NOT_RESTARTABLE.
 * @return 0 on success, -1 if the stack cannot be written.
 */
int arch_signal_frame_push(void *stack, size_t stack_size, struct address_space *space, int signal,
                           uintptr_t handler, uintptr_t restorer, uint32_t blocked, long restart_number);

/**
 * @brief Puts back what arch_signal_frame_push saved. Checks everything: the
 * frame is user memory.
 *
 * @param stack The user stack pointer where the signal frame was pushed.
 * @param stack_size The size of the user stack.
 * @param space The address space of the process.
 * @param blocked Receives the mask to restore.
 * @param result Receives the saved rax: the system call's return value overwrites it.
 * @return 0 on success, -1 if the frame is unreadable or invalid.
 */
int arch_signal_frame_pop(void *stack, size_t stack_size, struct address_space *space,
                          uint32_t *blocked, long *result);

#endif // _ARCH_SIGNAL_H_