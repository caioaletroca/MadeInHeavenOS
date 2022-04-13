#ifndef _STDIO_H_
#define _STDIO_H_

#include <stddef.h>
#include <stdarg.h>
#include <sys/cdefs.h>

#define EOF (-1)

__BEGIN_DECLS

int putchar(int c);

/**
 * @brief Outputs a formatted string using va_list variables
 * Returns a size - 1 characters plus string terminator
 * 
 * @param str Output string
 * @param size Maximum size allowed
 * @param format Formatation string
 * @param args Argument variables
 * @return int Output string size - 1
 */
int vsnprintf(char *str, size_t size, const char *format, va_list args);

__END_DECLS

#endif