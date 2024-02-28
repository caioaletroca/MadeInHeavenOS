#include "idt.h"
#include "io.h"
#include <kprintf.h>

#define ISR_NAME(index) isr_##index

#define IDT_INSTALL_CLI(index) ({ \
    extern void ISR_NAME(index)(void); \
    idt_entry_init(index, (uint64_t)ISR_NAME(index), 0x8E); \
})

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
void idt_entry_init(unsigned int i, uint64_t offset, uint8_t flags) {
    idt_entries[i].low_offset   = (uint64_t)offset & 0xFFFF;
    idt_entries[i].selector     = 0x08;                                     // Kernel Code Segment
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
    // Init the IDT register
    idt_reg.limit = sizeof(idt_entry) * 256 - 1;
    idt_reg.base = (uintptr_t)&idt_entries[0];
    
    // CPU interrupts to signal exceptions and faults.
    IDT_INSTALL_CLI(0);
    IDT_INSTALL_CLI(1);
    IDT_INSTALL_CLI(2);
    IDT_INSTALL_CLI(3);
    IDT_INSTALL_CLI(4);
    IDT_INSTALL_CLI(5);
    IDT_INSTALL_CLI(6);
    IDT_INSTALL_CLI(7);
    IDT_INSTALL_CLI(8);
    IDT_INSTALL_CLI(9);
    IDT_INSTALL_CLI(10);
    IDT_INSTALL_CLI(11);
    IDT_INSTALL_CLI(12);
    IDT_INSTALL_CLI(13);
    IDT_INSTALL_CLI(14);
    IDT_INSTALL_CLI(15);
    IDT_INSTALL_CLI(16);
    IDT_INSTALL_CLI(17);
    IDT_INSTALL_CLI(18);
    IDT_INSTALL_CLI(19);
    IDT_INSTALL_CLI(20);
    IDT_INSTALL_CLI(21);
    IDT_INSTALL_CLI(22);
    IDT_INSTALL_CLI(23);
    IDT_INSTALL_CLI(24);
    IDT_INSTALL_CLI(25);
    IDT_INSTALL_CLI(26);
    IDT_INSTALL_CLI(27);
    IDT_INSTALL_CLI(28);
    IDT_INSTALL_CLI(29);
    IDT_INSTALL_CLI(30);
    IDT_INSTALL_CLI(31);

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

    // Make effective by loading the new IDT register
    __asm__ __volatile__("lidt %0": : "m"(idt_reg));

    // __asm__ __volatile__("sti");
}