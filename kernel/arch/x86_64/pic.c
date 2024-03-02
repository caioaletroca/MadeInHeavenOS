#include "pic.h"
#include "io.h"

/**
 * Initialize PIC
*/
void pic_init() {
    pic_remap(PIC1, PIC2);
}

/**
 *  @brief Remap PIC
 *  @param offset1 Vector offset for master PIC vectors on the master become offset1..offset1+7
 *  @param offset2 Same for slave PIC: offset2..offset2+7
*/ 
void pic_remap(int offset1, int offset2) {
	unsigned char a1, a2;
 
	a1 = inb(PIC1_DATA);                        // Save masks
	a2 = inb(PIC2_DATA);
 
	outb(PIC1_COMMAND, ICW1_INIT | ICW1_ICW4);  // Starts the initialization sequence (in cascade mode)
	outb(PIC2_COMMAND, ICW1_INIT | ICW1_ICW4);
	outb(PIC1_DATA, offset1);                   // ICW2: Master PIC vector offset
	outb(PIC2_DATA, offset2);                   // ICW2: Slave PIC vector offset
	outb(PIC1_DATA, 4);                         // ICW3: tell Master PIC that there is a slave PIC at IRQ2 (0000 0100)
	outb(PIC2_DATA, 2);                         // ICW3: tell Slave PIC its cascade identity (0000 0010)
 
	outb(PIC1_DATA, ICW4_8086);
	outb(PIC2_DATA, ICW4_8086);
 
	outb(PIC1_DATA, a1);                        // Restore saved masks.
	outb(PIC2_DATA, a2);
}