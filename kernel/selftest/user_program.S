#include <syscall.h>

.section .rodata

/*
 * Program 1: count until the kernel sets `stop`, then exit(0).
 * Proves user code is preempted and can end itself.
 */
.globl user_loop_start, user_loop_counter, user_loop_stop, user_loop_end
user_loop_start:
1:
    incq user_loop_counter(%rip)
    cmpb $0, user_loop_stop(%rip)
    je 1b

    mov $SYS_EXIT, %eax
    xor %edi, %edi
    int $0x80
    ud2                                 # exit never returns

    .balign 8
user_loop_counter:
    .quad 0
user_loop_stop:
    .byte 0
user_loop_end:

/*
 * Program 2: print a greeting, try to write from a kernel address, store
 * that result where the kernel can check it, then exit with it.
 */
.globl user_hello_start, user_hello_result, user_hello_end
user_hello_start:
    mov $SYS_WRITE, %eax
    mov $1, %edi
    lea user_hello_message(%rip), %rsi
    mov $(user_hello_message_end - user_hello_message), %edx
    int $0x80

    # Kernel memory must be rejected with -EFAULT, not printed
    mov $SYS_WRITE, %eax
    mov $1, %edi
    movabs $0xFFFFFFFF80100000, %rsi
    mov $16, %edx
    int $0x80
    mov %rax, user_hello_result(%rip)

    mov %rax, %rdi
    mov $SYS_EXIT, %eax
    int $0x80
    ud2

user_hello_message:
    .ascii "Hello from ring 3!\n"
user_hello_message_end:

    .balign 8
user_hello_result:
    .quad 1                             # overwritten with the syscall result
user_hello_end: