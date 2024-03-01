#ifndef _STDIO_H_
#define _STDIO_H_

#include <stddef.h>
#include <stdarg.h>
#include <stdlib.h>
#include <sys/cdefs.h>
#include <string.h>

#define SEEK_SET 0
#define EOF (-1)

typedef struct { int unused } FILE;

__BEGIN_DECLS

extern FILE *stderr;

int fclose(FILE *);
int fflush(FILE *);
FILE *fopen(const char *, const char *);
int fprintf(FILE *, const char *, ...);
size_t fread(void *, size_t, size_t, FILE *);
int fseek(FILE *, long, int);
long ftell(FILE *);
size_t fwrite(const void *, size_t, size_t, FILE *);
void setbuf(FILE *, char *);
int vfprintf(FILE *, const char *, va_list);

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