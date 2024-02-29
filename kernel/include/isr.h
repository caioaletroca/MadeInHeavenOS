#ifndef _ARCH_X86_64_ISR_H_
#define _ARCH_X86_64_ISR_H_

#include <stdint.h>

typedef struct {
    uint64_t r11;
    uint64_t r10;
    uint64_t r9;
    uint64_t r8;
    uint64_t rcx;
    uint64_t rdx;
    uint64_t rsi;
    uint64_t rdi;
    uint64_t rax;
} isr_context;

void isr_handler(isr_context isr_reg);

#endif