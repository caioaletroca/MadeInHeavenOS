#ifndef _ARCH_IRQ_H_
#define _ARCH_IRQ_H_

/*
 * Architecture contract: hardware interrupt lines.
 *
 * IRQ numbers are interrupt-controller lines (0 = PC timer, 1 = PC keyboard),
 * never CPU vector numbers. The architecture maps them to vectors.
 */

typedef void (*irq_handler_t)(unsigned int irq);

/**
 * @brief Install the handler for an IRQ line and unmask it.
 *
 * @return 0 on success, or -1 if the line or handler is invalid.
 */
int irq_register(unsigned int irq, irq_handler_t handler);

#endif // _ARCH_IRQ_H_
