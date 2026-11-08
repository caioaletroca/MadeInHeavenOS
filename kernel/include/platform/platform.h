#ifndef _PLATFORM_PLATFORM_H_
#define _PLATFORM_PLATFORM_H_

#include <stddef.h>
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
 * @return The real length of a tick in nanoseconds (the hardware divides by a whole number).
 */
uint64_t platform_timer_init(uint32_t frequency);

typedef struct
{
    int year;  // e.g. 2026
    int month; // 1-12
    int day;   // 1-31
    int hour;  // 0-23
    int minute;
    int second;
} platform_date_t;

/**
 * @brief Read the battery-backed clock, assumed to hold UTC.
 *
 * @param date Output structure, fully overwritten with the current date and time in UTC.
 */
void platform_date_read(platform_date_t *date);

/**
 * @brief Unmask an IRQ line on the interrupt controller.
 */
void platform_irq_enable(unsigned int irq);

/**
 * @brief Acknowledge a handled IRQ on the interrupt controller.
 */
void platform_irq_eoi(unsigned int irq);

/**
 * @brief Write a string to the platform's debug output.
 *
 * @param s The string to write.
 * @param n The number of bytes to write.
 */
void platform_debug_write(const char *s, size_t n);

/**
 * @brief Power the machine off if the platform knows how.
 *
 * @note Returns if nothing worked (real hardware without ACPI support yet):
 *       the caller falls back to halting the CPU.
 */
void platform_power_off(void);

#endif // _PLATFORM_PLATFORM_H_
