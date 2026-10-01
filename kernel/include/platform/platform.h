#ifndef _PLATFORM_PLATFORM_H_
#define _PLATFORM_PLATFORM_H_

#include <stdint.h>

/*
 * Platform contract: every board under kernel/platform/ implements this.
 */

/**
 * @brief Initialize the interrupt controller and on-board devices.
 *
 * @note Must run after arch_init() and before interrupts are enabled.
 */
void platform_init(void);

/**
 * @brief Program the system timer and route its IRQ to timer_tick().
 *
 * @param frequency Tick frequency in Hz.
 */
void platform_timer_init(uint32_t frequency);

/**
 * @brief Unmask an IRQ line on the interrupt controller.
 */
void platform_irq_enable(unsigned int irq);

/**
 * @brief Acknowledge a handled IRQ on the interrupt controller.
 */
void platform_irq_eoi(unsigned int irq);

#endif // _PLATFORM_PLATFORM_H_
