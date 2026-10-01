#ifndef _PANIC_H_
#define _PANIC_H_

#include <kprintf.h>

/**
 * Prints a panic message and halts the system.
 *
 * @param fmt The format string for the panic message.
 * @param ... Additional arguments for the format string.
 */
__attribute__((noreturn, format(printf, 1, 2))) void panic(const char *fmt, ...);

#endif