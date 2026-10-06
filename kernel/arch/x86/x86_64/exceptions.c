#include <panic.h>
#include <x86/exceptions.h>
#include <x86/isr.h>
#include <sched/scheduler.h>
#include <sched/process.h>
#include <kprintf.h>
#include <signal.h>
#include <stdbool.h>

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
    "Security",
    "(Reserved exception 31)"};

// Signal that kills a user thread for each exception, as Linux maps them; 0 means SIGKILL
static const int exception_signals[32] = {
    [0] = SIGFPE,   // Division by zero
    [1] = SIGTRAP,  // Debug
    [3] = SIGTRAP,  // Breakpoint
    [4] = SIGSEGV,  // Overflow
    [5] = SIGSEGV,  // Bound range exceeded
    [6] = SIGILL,   // Invalid opcode
    [7] = SIGSEGV,  // Device not available (no FPU state handling yet)
    [10] = SIGSEGV, // Invalid TSS
    [11] = SIGBUS,  // Segment not present
    [12] = SIGBUS,  // Stack-Segment fault
    [13] = SIGSEGV, // General Protection Fault
    [14] = SIGSEGV, // Page Fault (handled in paging.c)
    [16] = SIGFPE,  // x87 Floating-Point
    [17] = SIGBUS,  // Alignment check
    [19] = SIGFPE,  // SIMD Floating-Point
    [21] = SIGSEGV, // Control Protection
};

/**
 * Check if an exception is considered fatal.
 *
 * @param int_no The interrupt number.
 * @return true if the exception is fatal, false otherwise.
 */
static bool exception_is_fatal(uint8_t int_no)
{
    return int_no == 2 || int_no == 8 || int_no == 18;
}

/**
 * Handle exceptions that occur in user mode.
 *
 * @param regs The ISR context.
 * @param int_no The interrupt number.
 */
__attribute__((noreturn)) static void exception_user_handler(isr_context_t *regs, uint8_t int_no)
{
    kprintf("user exception: %s (thread %u)\n"
            "\trip: %p, rsp: %p, err_code: %u\n",
            exception_messages[int_no], scheduler_current()->id,
            (void *)regs->rip, (void *)regs->rsp, (unsigned int)(regs->info & 0xFFFFFFFF));

    int signal = exception_signals[int_no] != 0 ? exception_signals[int_no] : SIGKILL;

    process_exit(SIGNAL_EXIT_STATUS(signal));
}

/**
 * Handle all exceptions.
 *
 * @param regs The ISR context.
 */
static void exception_handler(isr_context_t *regs)
{
    uint8_t int_no = (uint8_t)(regs->info >> 32) & 0xFF;

    if (int_no < 32)
    {
        if (isr_from_user(regs) && !exception_is_fatal(int_no))
            exception_user_handler(regs, int_no);

        panic(
            "Exception: %s\n"
            "\trip: %p, rsp: %p\n"
            "\tint_no: %u, err_code: %u",
            exception_messages[int_no],
            (void *)regs->rip,
            (void *)regs->rsp,
            int_no, (regs->info & 0xFFFFFFFF));
    }
    else
    {
        kprintf("IRQ: %u\n", int_no);
    }
}

void exception_init()
{
    for (uint8_t i = 0; i < 32; i++)
    {
        isr_info_t info = {
            .type = ISR_EXCEPTION,
            .handler = exception_handler,
        };

        isr_set_info(i, &info);
    }
}