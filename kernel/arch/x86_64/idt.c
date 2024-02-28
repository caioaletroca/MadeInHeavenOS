#include "idt.h"
#include "io.h"
#include <kprintf.h>

extern void isr_0(void);
extern void isr_1(void);
extern void isr_2(void);
extern void isr_3(void);
extern void isr_4(void);
extern void isr_5(void);
extern void isr_6(void);
extern void isr_7(void);
extern void isr_8(void);
extern void isr_9(void);
extern void isr_10(void);
extern void isr_11(void);
extern void isr_12(void);
extern void isr_13(void);
extern void isr_14(void);
extern void isr_15(void);
extern void isr_16(void);
extern void isr_17(void);
extern void isr_18(void);
extern void isr_19(void);
extern void isr_20(void);
extern void isr_21(void);
extern void isr_22(void);
extern void isr_23(void);
extern void isr_24(void);
extern void isr_25(void);
extern void isr_26(void);
extern void isr_27(void);
extern void isr_28(void);
extern void isr_29(void);
extern void isr_30(void);
extern void isr_31(void);
extern void isr_32(void);
extern void isr_33(void);
extern void isr_34(void);
extern void isr_35(void);
extern void isr_36(void);
extern void isr_37(void);
extern void isr_38(void);
extern void isr_39(void);
extern void isr_40(void);
extern void isr_41(void);
extern void isr_42(void);
extern void isr_43(void);
extern void isr_44(void);
extern void isr_45(void);
extern void isr_46(void);
extern void isr_47(void);

static idt_register idt_reg;

__attribute__((aligned(16)))
static idt_entry idt_entries[256];

// #define PIC1		0x20		/* IO base address for master PIC */
// #define PIC2		0xA0		/* IO base address for slave PIC */
// #define PIC1_COMMAND	PIC1
// #define PIC1_DATA	(PIC1+1)
// #define PIC2_COMMAND	PIC2
// #define PIC2_DATA	(PIC2+1)

// #define ICW1_ICW4	0x01		/* ICW4 (not) needed */
// #define ICW1_SINGLE	0x02		/* Single (cascade) mode */
// #define ICW1_INTERVAL4	0x04		/* Call address interval 4 (8) */
// #define ICW1_LEVEL	0x08		/* Level triggered (edge) mode */
// #define ICW1_INIT	0x10		/* Initialization - required! */
 
// #define ICW4_8086	0x01		/* 8086/88 (MCS-80/85) mode */
// #define ICW4_AUTO	0x02		/* Auto (normal) EOI */
// #define ICW4_BUF_SLAVE	0x08		/* Buffered mode/slave */
// #define ICW4_BUF_MASTER	0x0C		/* Buffered mode/master */
// #define ICW4_SFNM	0x10		/* Special fully nested (not) */
 
/*
arguments:
	offset1 - vector offset for master PIC
		vectors on the master become offset1..offset1+7
	offset2 - same for slave PIC: offset2..offset2+7
*/
// void PIC_remap(int offset1, int offset2)
// {
// 	unsigned char a1, a2;
 
// 	a1 = inb(PIC1_DATA);                        // save masks
// 	a2 = inb(PIC2_DATA);
 
// 	outb(PIC1_COMMAND, ICW1_INIT | ICW1_ICW4);  // starts the initialization sequence (in cascade mode)
// 	outb(PIC2_COMMAND, ICW1_INIT | ICW1_ICW4);
// 	outb(PIC1_DATA, offset1);                 // ICW2: Master PIC vector offset
// 	outb(PIC2_DATA, offset2);                 // ICW2: Slave PIC vector offset
// 	outb(PIC1_DATA, 4);                       // ICW3: tell Master PIC that there is a slave PIC at IRQ2 (0000 0100)
// 	outb(PIC2_DATA, 2);                       // ICW3: tell Slave PIC its cascade identity (0000 0010)
 
// 	outb(PIC1_DATA, ICW4_8086);
// 	outb(PIC2_DATA, ICW4_8086);
 
// 	outb(PIC1_DATA, a1);   // restore saved masks.
// 	outb(PIC2_DATA, a2);
// }

/**
 * @brief Initialize a single IDT entry
 * 
 * @param i 
 * @param offset 
 * @param selector 
 * @param flags 
 */
void idt_entry_init(unsigned int i, uint64_t offset, uint16_t selector, uint8_t flags) {
    idt_entries[i].low_offset   = (uint64_t)offset & 0xFFFF;
    idt_entries[i].selector     = selector;
    idt_entries[i].ist          = 0;
    idt_entries[i].flags        = flags;
    idt_entries[i].mid_offset   = ((uint64_t)offset >> 16) & 0xFFFF;
    idt_entries[i].high_offset  = ((uint64_t)offset >> 32) & 0xFFFFFFFF;
    idt_entries[i].zero         = 0;
}

// #define PIC_EOI		0x20

// void PIC_sendEOI(unsigned char irq)
// {
// 	if(irq >= 8)
// 		outb(PIC2_COMMAND,PIC_EOI);
 
// 	outb(PIC1_COMMAND,PIC_EOI);
// }

// void IRQ_set_mask(unsigned char IRQline) {
//     uint16_t port;
//     uint8_t value;
 
//     if(IRQline < 8) {
//         port = PIC1_DATA;
//     } else {
//         port = PIC2_DATA;
//         IRQline -= 8;
//     }
//     value = inb(port) | (1 << IRQline);
//     outb(port, value);        
// }
 
// void IRQ_clear_mask(unsigned char IRQline) {
//     uint16_t port;
//     uint8_t value;
 
//     if(IRQline < 8) {
//         port = PIC1_DATA;
//     } else {
//         port = PIC2_DATA;
//         IRQline -= 8;
//     }
//     value = inb(port) & ~(1 << IRQline);
//     outb(port, value);        
// }

void idt_init(void) {
    // uint32_t idt_addr = (uint32_t)&idt_entries;

    /* Init the IDT register */
    idt_reg.limit = sizeof(idt_entry) * 256 - 1;
    idt_reg.base = (uintptr_t)&idt_entries[0];

    // idt_entries[0].low_offset   = (uint64_t)isr_0 & 0xFFFF;
    // idt_entries[0].selector     = 0x08;
    // idt_entries[0].ist          = 0;
    // idt_entries[0].flags        = 0x8E;
    // idt_entries[0].mid_offset   = ((uint64_t)isr_0 >> 16) & 0xFFFF;
    // idt_entries[0].high_offset  = ((uint64_t)isr_0 >> 32) & 0xFFFFFFFF;
    // idt_entries[0].zero         = 0;

    // idt_entries[0].low_offset   = (uint64_t)isr_1 & 0xFFFF;
    // idt_entries[0].selector     = 0x08;
    // idt_entries[0].ist          = 0;
    // idt_entries[0].flags        = 0x8E;
    // idt_entries[0].mid_offset   = ((uint64_t)isr_1 >> 16) & 0xFFFF; 
    // idt_entries[0].high_offset  = ((uint64_t)isr_1 >> 32) & 0xFFFFFFFF;
    // idt_entries[0].zero         = 0;
    kprintf("%i", sizeof(idt_entry));
    // __asm__ __volatile__ ("xchgw %bx, %bx");
    /* CPU interrupts to signal exceptions and faults. */
    idt_entry_init(0,   (uint64_t)isr_0,    0x08,   0x8E);
    idt_entry_init(1,   (uint64_t)isr_1,    0x08,   0x8E);
    // idt_entry_init(2,   (uint64_t)isr_2,    0x08,   0x8E);
    // idt_entry_init(3,   (uint64_t)isr_3,    0x08,   0x8E);
    // idt_entry_init(4,   (uint64_t)isr_4,    0x08,   0x8E);
    // idt_entry_init(5,   (uint64_t)isr_5,    0x08,   0x8E);
    // idt_entry_init(6,   (uint64_t)isr_6,    0x08,   0x8E);
    // idt_entry_init(7,   (uint64_t)isr_7,    0x08,   0x8E);
    // idt_entry_init(8,   (uint64_t)isr_8,    0x08,   0x8E);
    // idt_entry_init(9,   (uint64_t)isr_9,    0x08,   0x8E);
    // idt_entry_init(10,  (uint64_t)isr_10,   0x08,   0x8E);
    // idt_entry_init(11,  (uint64_t)isr_11,   0x08,   0x8E);
    // idt_entry_init(12,  (uint64_t)isr_12,   0x08,   0x8E);
    // idt_entry_init(13,  (uint64_t)isr_13,   0x08,   0x8E);
    // idt_entry_init(14,  (uint64_t)isr_14,   0x08,   0x8E);
    // idt_entry_init(15,  (uint64_t)isr_15,   0x08,   0x8E);
    // idt_entry_init(16,  (uint64_t)isr_16,   0x08,   0x8E);
    // idt_entry_init(17,  (uint64_t)isr_17,   0x08,   0x8E);
    // idt_entry_init(18,  (uint64_t)isr_18,   0x08,   0x8E);
    // idt_entry_init(19,  (uint64_t)isr_19,   0x08,   0x8E);
    // idt_entry_init(20,  (uint64_t)isr_20,   0x08,   0x8E);
    // idt_entry_init(21,  (uint64_t)isr_21,   0x08,   0x8E);
    // idt_entry_init(22,  (uint64_t)isr_22,   0x08,   0x8E);
    // idt_entry_init(23,  (uint64_t)isr_23,   0x08,   0x8E);
    // idt_entry_init(24,  (uint64_t)isr_24,   0x08,   0x8E);
    // idt_entry_init(25,  (uint64_t)isr_25,   0x08,   0x8E);
    // idt_entry_init(26,  (uint64_t)isr_26,   0x08,   0x8E);
    // idt_entry_init(27,  (uint64_t)isr_27,   0x08,   0x8E);
    // idt_entry_init(28,  (uint64_t)isr_28,   0x08,   0x8E);
    // idt_entry_init(29,  (uint64_t)isr_29,   0x08,   0x8E);
    // idt_entry_init(30,  (uint64_t)isr_30,   0x08,   0x8E);
    // idt_entry_init(31,  (uint64_t)isr_31,   0x08,   0x8E);

    // PIC_remap(PIC1, PIC2);

    /* Devices interrupt requests (IRQs) */
    // idt_entry_init(32,  (uint64_t)isr_32,   0x08,   0x8E);
    // idt_entry_init(33,  (uint64_t)isr_33,   0x08,   0x8E);
    // idt_entry_init(34,  (uint64_t)isr_34,   0x08,   0x8E);
    // idt_entry_init(35,  (uint64_t)isr_35,   0x08,   0x8E);
    // idt_entry_init(36,  (uint64_t)isr_36,   0x08,   0x8E);
    // idt_entry_init(37,  (uint64_t)isr_37,   0x08,   0x8E);
    // idt_entry_init(38,  (uint64_t)isr_38,   0x08,   0x8E);
    // idt_entry_init(39,  (uint64_t)isr_39,   0x08,   0x8E);
    // idt_entry_init(40,  (uint64_t)isr_40,   0x08,   0x8E);
    // idt_entry_init(41,  (uint64_t)isr_41,   0x08,   0x8E);
    // idt_entry_init(42,  (uint64_t)isr_42,   0x08,   0x8E);
    // idt_entry_init(43,  (uint64_t)isr_43,   0x08,   0x8E);
    // idt_entry_init(44,  (uint64_t)isr_44,   0x08,   0x8E);
    // idt_entry_init(45,  (uint64_t)isr_45,   0x08,   0x8E);
    // idt_entry_init(46,  (uint64_t)isr_46,   0x08,   0x8E);
    // idt_entry_init(47,  (uint64_t)isr_47,   0x08,   0x8E);
    // idt_entry_init(48,  (uint64_t)isr_47,   0x08,   0x8E);
    
    /* Software interrupt (used by syscalls) */

    /* Make effective by loading the new IDT register */
    
    __asm__ __volatile__("lidt %0": : "m"(idt_reg));

    // __asm__ __volatile__("sti");
}