#ifndef _VECTORS_H_
#define _VECTORS_H_

// 0x00–0x1F: CPU exceptions (reserved by Intel)
#define VECTOR_EXCEPTION_BASE 0x00
#define VECTOR_EXCEPTION_COUNT 32

// 0x20–0x2F: legacy PIC IRQs (remapped)
#define VECTOR_IRQ_BASE 0x20
#define VECTOR_IRQ_COUNT 16

// Software interrupts
#define VECTOR_SYSCALL 0x80
#define VECTOR_SCHEDULER_YIELD 0x81

#endif // _VECTORS_H_