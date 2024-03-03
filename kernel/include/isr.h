#ifndef _ARCH_X86_64_ISR_H_
#define _ARCH_X86_64_ISR_H_

#include <stdint.h>
#include "panic.h"

typedef struct {
    // Register pushed by implementation
    uint64_t rax;
    uint64_t rdi;
    uint64_t rsi;
    uint64_t rdx;
    uint64_t rcx;
    uint64_t r8;
    uint64_t r9;
    uint64_t r10;
    uint64_t r11;

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
} isr_context;

/**
 * Defines the standard isr handler function
*/
typedef void (*isr_handler_t)(isr_context *regs);

/**
 * ISR Information structure
*/
typedef struct {
    enum isr_type {
        ISR_EXCEPTION,  // CPU exceptions
        ISR_IRQ         // Normal IRQs that requires EOI
    } type;
    isr_handler_t handler;
} isr_info_t;

/**
 * @brief Sets new items inside the isr_table vector
 * @param vector Index of the vector
 * @param info Struct information
*/
void isr_set_info(uint8_t vector, isr_info_t *info);

void isr_handler(isr_context *regs);

#endif