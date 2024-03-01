#ifndef _PANIC_H_
#define _PANIC_H_

#include <kprintf.h>

#define halt() __asm__ __volatile__ ("cli; hlt");

void panic(const char *fmt, ...);

#endif