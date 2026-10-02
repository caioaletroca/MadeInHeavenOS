#ifndef _ASM_X86_IRQ_FLAGS_H_
#define _ASM_X86_IRQ_FLAGS_H_

#include <stdint.h>

// RFLAGS.IF (see <x86/cpu.h>, which is private to arch code)
#define IRQ_FLAGS_ENABLED (1ULL << 9)

typedef uint64_t irq_flags_t;

/**
 * @brief Enable maskable interrupts on the current CPU.
 */
static inline void irq_enable(void)
{
    __asm__ __volatile__("sti" : : : "memory");
}

/**
 * @brief Check if maskable interrupts are enabled on the current CPU.
 *
 * @return Non-zero if interrupts are enabled, zero otherwise.
 */
static inline int irq_enabled(void)
{
    irq_flags_t flags;
    __asm__ __volatile__("pushfq\n\tpopq %0" : "=r"(flags) : : "memory");
    return (flags & IRQ_FLAGS_ENABLED) != 0;
}

/**
 * @brief Disable maskable interrupts on the current CPU.
 */
static inline void irq_disable(void)
{
    __asm__ __volatile__("cli" : : : "memory");
}

/**
 * @brief Disable interrupts and return the previous interrupt state.
 */
static inline irq_flags_t irq_save(void)
{
    irq_flags_t flags;
    __asm__ __volatile__("pushfq\n\tpopq %0\n\tcli" : "=r"(flags) : : "memory");
    return flags;
}

/**
 * @brief Restore the interrupt state returned by irq_save().
 */
static inline void irq_restore(irq_flags_t flags)
{
    if (flags & IRQ_FLAGS_ENABLED)
        irq_enable();
}

#endif // _ASM_X86_IRQ_FLAGS_H_
