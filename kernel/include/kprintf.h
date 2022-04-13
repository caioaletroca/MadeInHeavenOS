#ifndef _KPRINTF_H_
#define _KPRINTF_H_

#include <stdarg.h>
#include <driver/tty.h>

void kvprintf(const char *fmt, va_list args);
void kprintf(const char *fmt, ...);

#endif