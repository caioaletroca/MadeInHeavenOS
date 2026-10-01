#ifndef _PLATFORM_PLATFORM_H_
#define _PLATFORM_PLATFORM_H_

#include <stdint.h>
#include <boot_info.h>

/**
 * @brief Translate the bootloader handoff into a boot_info_t.
 *
 * @param handoff Value passed by the arch entry code to kmain (on PC: the
 *                physical address of the Multiboot2 information structure).
 * @param info    Output structure, fully overwritten.
 *
 * @note Runs first in kmain, before any other initialization.
 */
void platform_boot_info_init(uintptr_t handoff, boot_info_t *boot_info);

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
