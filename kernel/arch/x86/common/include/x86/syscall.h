#ifndef _X86_SYSCALL_H
#define _X86_SYSCALL_H

/**
 * @brief Route int 0x80 to the generic syscall dispatcher.
 */
void syscall_init(void);

#endif // _X86_SYSCALL_H