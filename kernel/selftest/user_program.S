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

/*
 * Program 3: mark that it ran, read address 0 (page 0 is never mapped),
 * then mark that it survived. The kernel must kill it before the second mark.
 */
.globl user_null_start, user_null_before, user_null_after, user_null_end
user_null_start:
    movb $1, user_null_before(%rip)
    mov 0, %rax                         # absolute address 0, not RIP-relative
    movb $1, user_null_after(%rip)
1:  jmp 1b
    .balign 8
user_null_before:
    .quad 0
user_null_after:
    .quad 0
user_null_end:

/*
 * Program 4: same as program 3, but runs `hlt`, a privileged instruction,
 * so the kill goes through the general protection fault path.
 */
.globl user_hlt_start, user_hlt_before, user_hlt_after, user_hlt_end
user_hlt_start:
    movb $1, user_hlt_before(%rip)
    hlt                                 # #GP(0) at CPL 3
    movb $1, user_hlt_after(%rip)
1:  jmp 1b
    .balign 8
user_hlt_before:
    .quad 0
user_hlt_after:
    .quad 0
user_hlt_end:

/*
 * Program 5: exercise the fd table. Each call stores its result in the
 * next slot of user_files_results (checked by the kernel), then exit(0).
 */
.macro files_write fd, slot
    mov $SYS_WRITE, %eax
    mov $\fd, %edi
    lea user_files_message(%rip), %rsi
    mov $(user_files_message_end - user_files_message), %edx
    int $0x80
    mov %rax, user_files_results+\slot*8(%rip)
.endm

.macro files_close fd, slot
    mov $SYS_CLOSE, %eax
    mov $\fd, %edi
    int $0x80
    mov %rax, user_files_results+\slot*8(%rip)
.endm

.globl user_files_start, user_files_results, user_files_message, user_files_message_end, user_files_end
user_files_start:
    files_write 7, 0                    # empty slot: -EBADF
    files_write 100, 1                  # out of range: -EBADF
    files_close 2, 2                    # 0
    files_write 2, 3                    # just closed: -EBADF
    files_close 2, 4                    # double close: -EBADF
    files_write 1, 5                    # stdout survives fd 2 closing: full length

    mov $SYS_EXIT, %eax
    xor %edi, %edi
    int $0x80
    ud2

user_files_message:
    .ascii "Files test: stdout still works\n"
user_files_message_end:

    .balign 8
user_files_results:
    .quad 1, 1, 1, 1, 1, 1              # never a valid result, so a skipped store is caught
user_files_end: