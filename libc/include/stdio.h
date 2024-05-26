#ifndef _STDIO_H_
#define _STDIO_H_

#include <stddef.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/cdefs.h>
#include <string.h>

// IO buffer size.
#define BUFFER_SIZE 512

#define SEEK_SET 0
#define EOF (-1)

// Opaque FILE type
typedef struct _FILE FILE;

__BEGIN_DECLS

extern FILE *stdin;
extern FILE *stdout;
extern FILE *stderr;

int fclose(FILE *);
int fflush(FILE *);
FILE *fopen(const char *, const char *);
int fprintf(FILE *, const char *, ...);
size_t fread(void *, size_t, size_t, FILE *);
int fseek(FILE *, long, int);
long ftell(FILE *);
void setbuf(FILE *, char *);

int putchar(int c);

int fputc(int c, FILE *stream);

/**
 * @brief Writes an array of count elements, each one with a size of size bytes,
 * from the block of memory pointed by ptr to the current position in the stream.
 * The position indicator of the stream is advanced by the total number of bytes written.
 * 
 * Internally, the function interprets the block pointed by ptr as if it was an array of (size*count) elements of type unsigned char,
 * and writes them sequentially to stream as if fputc was called for each byte.
 * 
 * @param ptr Pointer to the array of elements to be written, converted to a const void*.
 * @param size Size in bytes of each element to be written.
 * @param count Number of elements, each one with a size of size bytes.
 * @param stream Pointer to a FILE object that specifies an output stream.
 * @return The total number of elements successfully written is returned. 
 * If this number differs from the count parameter, a writing error prevented the function from completing.
*/
size_t fwrite(const void *ptr, size_t size, size_t count, FILE *stream);

/**
 * @brief Writes the C string pointed by format to the stream,
 * replacing any format specifier in the same way as printf does,
 * but using the elements in the variable argument list identified by arg instead of additional function arguments.
 * 
 * @param stream Pointer to a FILE object that identifies an output stream.
 * @param format C string that contains a format string that follows the same specifications as format in printf.
 * @param args A value identifying a variable arguments list initialized with va_start
 * @return On success, the total number of characters written is returned.
*/
int vfprintf(FILE *stream, const char *format, va_list args);

/**
 * @brief Writes the C string pointed by format to the standard output (stdout),
 * replacing any format specifier in the same way as printf does,
 * but using the elements in the variable argument list identified by arg instead of additional function arguments.
 * 
 * @param format C string that contains a format string that follows the same specifications as format in printf.
 * @param args A value identifying a variable arguments list initialized with va_start
 * @return On success, the total number of characters written is returned.
*/
int vprintf(const char *format, va_list args);

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