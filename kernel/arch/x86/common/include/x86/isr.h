#ifndef _ARCH_X86_64_ISR_H_
#define _ARCH_X86_64_ISR_H_

#include <stdint.h>
#include <stdbool.h>

typedef struct isr_context
{
    // Pushed by isr_common (last push = lowest address = first field)
    uint64_t rax;
    uint64_t rbx;
    uint64_t rcx;
    uint64_t rdx;
    uint64_t rsi;
    uint64_t rdi;
    uint64_t rbp;
    uint64_t r8;
    uint64_t r9;
    uint64_t r10;
    uint64_t r11;
    uint64_t r12;
    uint64_t r13;
    uint64_t r14;
    uint64_t r15;

    // Information pushed by isr implementations
    // First 8 low bytes is error code
    // Last 8 higher bytes the interrupt number
    uint64_t info;

    // Interrupt stack frame pushed by CPU
    uint64_t rip;
    uint64_t cs;
    uint64_t rflags;
    uint64_t rsp;
    uint64_t ss;
} isr_context_t;

/**
 * Defines the standard isr handler function
 */
typedef void (*isr_handler_t)(isr_context_t *regs);

/**
 * ISR Information structure
 */
typedef struct
{
    enum isr_type
    {
        ISR_EXCEPTION, // CPU exceptions
        ISR_IRQ        // Normal IRQs that requires EOI
    } type;
    isr_handler_t handler;
} isr_info_t;

/**
 * Check if the interrupt originated from user mode.
 *
 * @param regs The ISR context.
 * @return true if the interrupt originated from user mode, false otherwise.
 */
static inline bool isr_from_user(isr_context_t *regs)
{
    return (regs->cs & 3) == 3;
}

/**
 * @brief Sets new items inside the isr_table vector
 * @param vector Index of the vector
 * @param info Struct information
 */
void isr_set_info(uint8_t vector, isr_info_t *info);

/**
 * @brief Common ISR handler called by assembly stubs
 * @param ctx Pointer to the ISR context
 * @return Pointer to the ISR context
 */
isr_context_t *isr_handler(isr_context_t *ctx);

#endif