#ifndef _IRQ_FLAGS_H_
#define _IRQ_FLAGS_H_

#include <stdint.h>
#include <cpu.h>

/**
 * @brief Disable interrupts and return the previous RFLAGS.
 */
static inline uint64_t irq_save(void)
{
    uint64_t flags;
    __asm__ __volatile__("pushfq\n\tpopq %0\n\tcli" : "=r"(flags) : : "memory");
    return flags;
}

/**
 * @brief Restore IF from a value returned by irq_save().
 */
static inline void irq_restore(uint64_t flags)
{
    if (flags & RFLAGS_IF)
        __asm__ __volatile__("sti" : : : "memory");
}

#endif // _IRQ_FLAGS_H_