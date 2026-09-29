.section .text
.globl context_switch

.code64

.type context_switch, @function
context_switch:
    # Save callee-saved registers and switch context
    pushq %rbp
    pushq %rbx
    pushq %r12
    pushq %r13
    pushq %r14
    pushq %r15

    # Save the current stack pointer to the old context
    movq %rsp, (%rdi)
    movq %rsi, %rsp

    # Load the new stack pointer from the new context
    popq %r15
    popq %r14
    popq %r13
    popq %r12
    popq %rbx
    popq %rbp

    ret
.size context_switch, .-context_switch