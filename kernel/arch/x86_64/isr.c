#include "isr.h"

static const char *const exception_messages[32] = {
    "Division by zero",
    "Debug",
    "Non-maskable Interrupt",
    "Breakpoint",
    "Overflow",
    "Bound range exceeded",
    "Invalid opcode",
    "Device not available",
    "Double Fault",
    "(Reserved exception 9)",
    "Invalid TSS",
    "Segment not present",
    "Stack-Segment fault",
    "General Protection Fault",
    "Page Fault",
    "(Reserved exception 15)",
    "x87 Floating-Point",
    "Alignment check",
    "Machine check",
    "SIMD Floating-Point",
    "Virtualization",
    "Control Protection",
    "(Reserved exception 22)",
    "(Reserved exception 23)",
    "(Reserved exception 24)",
    "(Reserved exception 25)",
    "(Reserved exception 26)",
    "(Reserved exception 27)",
    "Hypervisor Injection",
    "VMM Communication",
    "Secutiry",
    "(Reserved exception 31)"
};

static void exception_handler(isr_context *regs) {
    uint8_t int_no = (uint8_t)(regs->info >> 32) & 0xFF;

    panic(
        "Exception: %s\n"
        "\trip: %p, rsp: %p\n"
        "\tint_no: %i, err_code: %lu",
        exception_messages[int_no],
        (void *)regs->rip, (void *)regs->rsp,
        int_no, (regs->info & 0xFFFFFFFF)
    );

    __asm__ __volatile__ ("xchgw %bx, %bx");
}

void isr_handler(isr_context *regs) {
    // uint8_t int_no = (regs->info >> 32) & 0xFF;
    // uint8_t err_code = regs->info & 0xFF;

    exception_handler(regs);
}