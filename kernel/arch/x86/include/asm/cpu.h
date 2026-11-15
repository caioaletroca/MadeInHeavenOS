#ifndef _ASM_X86_CPU_H_
#define _ASM_X86_CPU_H_

#include <stdint.h>

#define MSR_FS_BASE 0xC0000100 // the FS segment's base: user mode's thread pointer

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

/**
 * @brief Load a user thread's thread pointer into the CPU (x86-64: the FS base).
 * The address must be canonical: wrmsr faults on any other.
 */
static inline void cpu_thread_pointer_load(uintptr_t pointer)
{
    __asm__ __volatile__("wrmsr" ::"c"(MSR_FS_BASE), "a"((uint32_t)pointer), "d"((uint32_t)(pointer >> 32))
                         : "memory");
}

#endif // _ASM_X86_CPU_H_
