#include "isr.h"
#include <kprintf.h>

void isr_handler(isr_context *regs) {
    uint8_t int_no = (regs->info >> 32) & 0xFF;
    uint8_t err_code = regs->info & 0xFF;
    
    kprintf("int_no: %i\n", int_no);
    kprintf("err_code: %i\n", err_code);

    // kprintf("rax: %i\n", regs->rax);
    // kprintf("rdi: %i\n", regs->rdi);
    // kprintf("rsi: %i\n", regs->rsi);
    // kprintf("rcx: %i\n", regs->rcx);
    // kprintf("r8: %i\n", regs->r8);
    // kprintf("r9: %i\n", regs->r9);
    // kprintf("r10: %i\n", regs->r10);
    // kprintf("r11: %i\n", regs->r11);
    
    // kprintf("info: %i\n", regs->info);

    // kprintf("rip: %i\n", regs->rip);
    // kprintf("cs: %i\n", regs->cs);
    // kprintf("rflags: %i\n", regs->rflags);
    // kprintf("rsp: %i\n", regs->rsp);
    // kprintf("ss: %i\n", regs->ss);

    __asm__ __volatile__ ("xchgw %bx, %bx");
}