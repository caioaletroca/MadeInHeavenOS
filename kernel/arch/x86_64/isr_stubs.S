.extern isr_handler

/* 
 * This macro creates a stub for an ISR which does not pass it's own
 * error code. Adds a dummy error code.
 */
.macro ISR num
    .globl isr_\num
    isr_\num:
        pushq $0
        # Store  the interrupt number in the highest four bytes of the erro code.
        # This way we can always increment rsp by 8 before iretq and no memory is wasted.
        movl $\num, 4(%rsp)
        jmp isr_common
.endm

/*
 * This macro creates a stub for an ISR which passes it's own error code
 */
.macro ISR_ERR num
    .globl isr_\num
    isr_\num:
        # Store  the interrupt number in the highest four bytes of the erro code.
        # This way we can always increment rsp by 8 before iretq and no memory is wasted.
        movl $\num, 4(%rsp)
        jmp isr_common
.endm

.macro ISR_IRQ num
    .globl isr_\num
    isr_\num:
        pushq $\num
        jmp isr_common
.endm

.section .text

.code64

isr_common:
    # Preserve Scratch Register in order
    pushq %r11
    pushq %r10
    pushq %r9
    pushq %r8
    pushq %rcx
    pushq %rdx
    pushq %rsi
    pushq %rdi
    pushq %rax

    mov %rsp, %rdi
    call isr_handler

    # Pop back register
    popq %rax
    popq %rdi
    popq %rsi
    popq %rdx
    popq %rcx
    popq %r8
    popq %r9
    popq %r10
    popq %r11

    # Remove info from the stack
    add $8, %rsp

    iretq

/*
 * Define all the required ISR stubs
 */
ISR 0
ISR 1
ISR 2
ISR 3
ISR 4
ISR 5
ISR 6
ISR 7
ISR_ERR 8
ISR 9
ISR_ERR 10
ISR_ERR 11
ISR_ERR 12
ISR_ERR 13
ISR_ERR 14
ISR 15
ISR 16
ISR 17
ISR 18
ISR 19
ISR 20
ISR 21
ISR 22
ISR 23
ISR 24
ISR 25
ISR 26
ISR 27
ISR 28
ISR 29
ISR 30
ISR 31
ISR_IRQ 32
ISR_IRQ 33
ISR_IRQ 34
ISR_IRQ 35
ISR_IRQ 36
ISR_IRQ 37
ISR_IRQ 38
ISR_IRQ 39
ISR_IRQ 40
ISR_IRQ 41
ISR_IRQ 42
ISR_IRQ 43
ISR_IRQ 44
ISR_IRQ 45
ISR_IRQ 46
ISR_IRQ 47