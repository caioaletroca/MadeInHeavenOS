#include "pic.h"
#include "io.h"

void pic_disable(void)
{
	outb(PIC1_DATA, 0xfd);
	outb(PIC2_DATA, 0xff);
}

void pic_init()
{
	// Remaps PIC chip to offset outside of CPU interrupts
	pic_remap(PIC1_VECTOR_OFFSET, PIC2_VECTOR_OFFSET);
	pic_disable();
}

void pic_remap(int offset1, int offset2)
{
	unsigned char a1, a2;

	// Save masks
	a1 = inb(PIC1_DATA);
	a2 = inb(PIC2_DATA);

	// Starts the initialization sequence (in cascade mode)
	outb(PIC1_COMMAND, ICW1_INIT | ICW1_ICW4);
	outb(PIC2_COMMAND, ICW1_INIT | ICW1_ICW4);

	// Send PIC vector offsets
	outb(PIC1_DATA, offset1);
	outb(PIC2_DATA, offset2);

	// ICW3: tell Master PIC that there is a slave PIC at IRQ2 (0000 0100)
	outb(PIC1_DATA, 4);
	// ICW3: tell Slave PIC its cascade identity (0000 0010)
	outb(PIC2_DATA, 2);

	outb(PIC1_DATA, ICW4_8086);
	outb(PIC2_DATA, ICW4_8086);

	// Restore saved masks.
	outb(PIC1_DATA, a1);
	outb(PIC2_DATA, a2);
}

void pic_irq_enable(uint8_t irq)
{
	uint16_t port = (irq < 8) ? PIC1_DATA : PIC2_DATA;
	uint8_t mask = inb(port) & ~(1 << (irq % 8));

	outb(port, mask);
}

void pic_irq_disable(uint8_t irq)
{
	uint16_t port = (irq < 8) ? PIC1_DATA : PIC2_DATA;
	uint8_t mask = inb(port) | (1 << (irq % 8));

	outb(port, mask);
}

void pic_send_EOI(unsigned char irq)
{
	if (irq >= 8)
		outb(PIC2_COMMAND, PIC_EOI);

	outb(PIC1_COMMAND, PIC_EOI);
}