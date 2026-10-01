#ifndef _ASM_X86_CPU_H_
#define _ASM_X86_CPU_H_

/**
 * @brief Sleep until the next interrupt.
 */
static inline void cpu_idle(void)
{
    __asm__ __volatile__("hlt" : : : "memory");
}

/**
 * @brief Stop the current CPU for good: interrupts off, then halt.
 */
__attribute__((noreturn)) static inline void cpu_halt(void)
{
    for (;;)
        __asm__ __volatile__("cli; hlt" : : : "memory");
}

/**
 * @brief Bochs magic breakpoint (no-op on real hardware).
 */
static inline void cpu_breakpoint(void)
{
    __asm__ __volatile__("xchgw %bx, %bx");
}

#endif // _ASM_X86_CPU_H_
