#include <syscall.h>

.section .rodata

/*
 * The one raw user program: count until the kernel sets `stop`, then exit(0).
 * Proves user code that never makes a syscall is preempted, and can end itself;
 * no libc test can check that (nothing in user space could stop a looping child).
 * The rest is tested by the libc programs in user/tests/.
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
