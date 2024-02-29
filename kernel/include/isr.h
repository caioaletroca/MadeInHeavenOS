#ifndef _ARCH_X86_64_ISR_H_
#define _ARCH_X86_64_ISR_H_

#include <stdint.h>

typedef struct {
    uint64_t rax;
    uint64_t rdi;
    uint64_t rsi;
    uint64_t rdx;
    uint64_t rcx;
    uint64_t r8;
    uint64_t r9;
    uint64_t r10;
    uint64_t r11;

    uint64_t info;

    // Interrupt stack frame
	uint64_t rip;
	uint64_t cs;
	uint64_t rflags;
	uint64_t rsp;
	uint64_t ss;
} isr_context;

void isr_handler(isr_context *regs);

#endif