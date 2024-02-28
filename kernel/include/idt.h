#ifndef _ARCH_X86_64_IDT_H_
#define _ARCH_X86_64_IDT_H_

#include <stdint.h>

/**
 * @brief Interrupt descriptor table entry
 */
typedef struct {
    uint16_t    low_offset;         // The lower 16 bits of the ISR's address
    uint16_t    selector;           // Kernel segment selector
    uint8_t     ist;                // 
    uint8_t     flags;              //
    uint16_t    mid_offset;         // The higher 16 bits of the lower 32 bits of the ISR's address
    uint32_t    high_offset;        // The higher 32 bits of the ISR's address
    uint32_t    zero;               // Reserved, zero
} __attribute__((__packed__)) idt_entry;

/**
 * @brief A struct describing a pointer to an array of interrupt handlers.
 * This is in a format suitable for giving to 'lidt'.
 */
typedef struct {
    uint16_t    limit;              //
    uint64_t    base;               // Address of the first entry
} __attribute__((__packed__)) idt_register;

/**
 * @brief Initialize the Interrupt Descriptor Table
 * 
 */
void idt_init(void);

#endif