#ifndef _ARCH_X86_64_PIC_H_
#define _ARCH_X86_64_PIC_H_

#include <stdint.h>
#include <vectors.h>

#define PIC1 0x20 /* IO base address for master PIC */
#define PIC2 0xA0 /* IO base address for slave PIC */
#define PIC1_COMMAND PIC1
#define PIC1_DATA (PIC1 + 1)
#define PIC2_COMMAND PIC2
#define PIC2_DATA (PIC2 + 1)

#define ICW1_ICW4 0x01      /* ICW4 (not) needed */
#define ICW1_SINGLE 0x02    /* Single (cascade) mode */
#define ICW1_INTERVAL4 0x04 /* Call address interval 4 (8) */
#define ICW1_LEVEL 0x08     /* Level triggered (edge) mode */
#define ICW1_INIT 0x10      /* Initialization - required! */

#define ICW4_8086 0x01       /* 8086/88 (MCS-80/85) mode */
#define ICW4_AUTO 0x02       /* Auto (normal) EOI */
#define ICW4_BUF_SLAVE 0x08  /* Buffered mode/slave */
#define ICW4_BUF_MASTER 0x0C /* Buffered mode/master */
#define ICW4_SFNM 0x10       /* Special fully nested (not) */

#define PIC1_VECTOR_OFFSET VECTOR_IRQ_BASE
#define PIC2_VECTOR_OFFSET 0x28

/**
 *  @brief Get the interrupt vector for a given IRQ line
 *
 *  @param irq The IRQ line number
 */
#define PIC_IRQ_VECTOR(irq) \
    (PIC1_VECTOR_OFFSET + (irq))

// IRQ line definitions for common devices
#define PIC_IRQ_TIMER 0
#define PIC_IRQ_KEYBOARD 1

#define PIC_EOI 0x20

/**
 * Initialize PIC
 */
void pic_init(void);

/**
 *  @brief Remap PIC
 *  @param offset1 Vector offset for master PIC vectors on the master become offset1..offset1+7
 *  @param offset2 Same for slave PIC: offset2..offset2+7
 */
void pic_remap(int offset1, int offset2);

/**
 *  @brief Enable a specific IRQ line on the PIC
 *  @param irq The IRQ line to enable
 */
void pic_irq_enable(uint8_t irq);

/**
 *  @brief Disable a specific IRQ line on the PIC
 *  @param irq The IRQ line to disable
 */
void pic_irq_disable(uint8_t irq);

/**
 *  @brief Send an end-of-interrupt command for an IRQ line
 *  @param irq The IRQ line that completed handling
 */
void pic_send_EOI(uint8_t irq);

#endif