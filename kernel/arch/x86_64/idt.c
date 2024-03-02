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

    // Devices interrupt requests (IRQs)
    IDT_INSTALL_CLI(32);
    IDT_INSTALL_CLI(33);
    IDT_INSTALL_CLI(34);
    IDT_INSTALL_CLI(35);
    IDT_INSTALL_CLI(36);
    IDT_INSTALL_CLI(37);
    IDT_INSTALL_CLI(38);
    IDT_INSTALL_CLI(39);
    IDT_INSTALL_CLI(40);
    IDT_INSTALL_CLI(41);
    IDT_INSTALL_CLI(42);
    IDT_INSTALL_CLI(43);
    IDT_INSTALL_CLI(44);
    IDT_INSTALL_CLI(45);
    IDT_INSTALL_CLI(46);
    IDT_INSTALL_CLI(47);
    
    /* Software interrupt (used by syscalls) */
    // TODO: Add to 128 syscall
}

/**
 * Loads IDT structure and enable interrupts
*/
void idt_load() {
    __asm__ __volatile__("lidt %0": : "m"(idt_reg));
    __asm__ __volatile__("sti");
}