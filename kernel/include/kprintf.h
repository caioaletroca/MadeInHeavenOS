#ifndef _KPRINTF_H_
#define _KPRINTF_H_

#include <stdarg.h>
#include <stdio.h>
#include <driver/tty.h>

/**
 * @brief kprintf internal usage
 * 
 * @param fmt 
 * @param args 
 */
void kvprintf(const char *fmt, va_list args);

/**
 * @brief Prints a formatted string into the console
 * 
 * @param fmt Format string
 * @param ... 
 */
void kprintf(const char *fmt, ...);

#endif