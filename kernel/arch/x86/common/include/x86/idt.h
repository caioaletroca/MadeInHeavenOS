#ifndef _ARCH_X86_64_IDT_H_
#define _ARCH_X86_64_IDT_H_

#include <stdint.h>

#define IDT_ENTRIES 256

// Gate attribute byte: P | DPL(2) | 0 | Type(4)
#define IDT_ATTR_PRESENT 0x80
#define IDT_ATTR_DPL(dpl) (((dpl) & 0x3) << 5)
#define IDT_TYPE_INTERRUPT_GATE 0xE // Clears IF on entry
#define IDT_TYPE_TRAP_GATE 0xF      // Leaves IF unchanged

#define IDT_GATE_INTERRUPT \
    (IDT_ATTR_PRESENT | IDT_ATTR_DPL(0) | IDT_TYPE_INTERRUPT_GATE) // 0x8E
#define IDT_GATE_TRAP \
    (IDT_ATTR_PRESENT | IDT_ATTR_DPL(0) | IDT_TYPE_TRAP_GATE) // 0x8F
#define IDT_GATE_USER_INTERRUPT \
    (IDT_ATTR_PRESENT | IDT_ATTR_DPL(3) | IDT_TYPE_INTERRUPT_GATE) // 0xEE, for syscalls later

/**
 * @brief Interrupt descriptor table entry
 */
typedef struct
{
    uint16_t low_offset;  // The lower 16 bits of the ISR's address
    uint16_t selector;    // Kernel segment selector
    uint8_t ist;          //
    uint8_t flags;        //
    uint16_t mid_offset;  // The higher 16 bits of the lower 32 bits of the ISR's address
    uint32_t high_offset; // The higher 32 bits of the ISR's address
    uint32_t zero;        // Reserved, zero
} __attribute__((__packed__)) idt_entry_t;

/**
 * @brief A struct describing a pointer to an array of interrupt handlers.
 * This is in a format suitable for giving to 'lidt'.
 */
typedef struct
{
    uint16_t limit; //
    uint64_t base;  // Address of the first entry
} __attribute__((__packed__)) idt_register_t;

/**
 * @brief Initialize the Interrupt Descriptor Table
 *
 */
void idt_init(void);

/**
 * @brief Loads IDT structure
 */
void idt_load(void);

#endif