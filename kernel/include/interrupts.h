#ifndef _INTERRUPTS_H_
#define _INTERRUPTS_H_

#include "idt.h"
#include "pic.h"

/**
 * Initialize the interrupt sequence setup
*/
void interrupts_init(void);

#endif