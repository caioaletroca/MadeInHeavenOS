#ifndef _ARCH_ARCH_H_
#define _ARCH_ARCH_H_

/*
 * Architecture contract: every arch under kernel/arch/ implements this.
 */

/**
 * @brief Initialize CPU-level state: exception/interrupt tables and
 * architecture-owned handlers (faults, scheduler yield, ...).
 *
 * @note Interrupts are still disabled when this returns.
 */
void arch_init(void);

#endif // _ARCH_ARCH_H_
