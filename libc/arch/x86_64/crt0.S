#include <mihos/syscall.h>

/*
 * Program entry: the kernel starts here with RSP = argc,
 * `call` pushes the return address, so main sees the SysV ABI's
 * RSP + 16 alignment. main's return value becomes the exit status.
 */
.text
.code64
.globl _start
.type _start, @function
_start:
    xor %ebp, %ebp                      # end of the frame-pointer chain for debuggers
    mov (%rsp), %rdi                    # argc
    lea 8(%rsp), %rsi                   # argv
    lea 16(%rsp, %rdi, 8), %rdx          # envp
    mov %rdx, environ(%rip)             # for getenv
    call main
    mov %eax, %edi                      # exit(main's return value)
    call exit
    ud2                                 # exit never returns
.size _start, . - _start
