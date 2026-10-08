#ifndef _STDIO_H_
#define _STDIO_H_

#include <stddef.h>
#include <stdarg.h>
#include <stdint.h>
#include <sys/cdefs.h>

// IO buffer size.
#define BUFSIZ 512

#define SEEK_SET 0
#define EOF (-1)

// Opaque FILE type
typedef struct _FILE FILE;

__BEGIN_DECLS

extern FILE *const stdin;
extern FILE *const stdout;
extern FILE *const stderr;

int fclose(FILE *);

/**
 * @brief Flushes the output buffer of a stream.
 *
 * @param stream Pointer to a FILE object that identifies the stream to be flushed.
 * @return On success, 0 is returned. On error, EOF is returned.
 */
int fflush(FILE *);

/**
 * @brief Checks the error indicator of a stream.
 *
 * @param stream Pointer to a FILE object that identifies the stream.
 * @return Non-zero if the error indicator is set, 0 otherwise.
 */
int ferror(FILE *stream);

FILE *fopen(const char *, const char *);

size_t fread(void *, size_t, size_t, FILE *);
int fseek(FILE *, long, int);
long ftell(FILE *);
void setbuf(FILE *, char *);

/**
 * @brief Checks the end-of-file indicator for the given stream.
 *
 * @param stream Pointer to a FILE object that identifies the stream.
 * @return Non-zero if the end-of-file indicator is set, 0 otherwise.
 */
int feof(FILE *stream);

/**
 * @brief Clears the end-of-file and error indicators for the given stream.
 *
 * @param stream Pointer to a FILE object that identifies the stream.
 */
void clearerr(FILE *stream);

/**
 * @brief Reads the next character from the specified stream.
 *
 * @param stream Pointer to a FILE object that identifies the stream.
 * @return On success, the character read is returned as an unsigned char cast to an int.
 * On end of file or error, EOF is returned.
 */
int fgetc(FILE *stream);

/**
 * @brief Reads the next character from the specified stream.
 *
 * @param stream Pointer to a FILE object that identifies the stream.
 * @return On success, the character read is returned as an unsigned char cast to an int.
 * On end of file or error, EOF is returned.
 */
int getc(FILE *stream);

/**
 * @brief Reads the next character from the standard input (stdin).
 *
 * @return On success, the character read is returned as an unsigned char cast to an int.
 * On end of file or error, EOF is returned.
 */
int getchar(void);

/**
 * @brief Pushes the character c (converted to an unsigned char) back onto the input stream pointed to by stream.
 *
 * @param c Character to be pushed back.
 * @param stream Pointer to a FILE object that identifies the stream.
 * @return On success, the character pushed back is returned. On error, EOF is returned.
 */
int ungetc(int c, FILE *stream);

/**
 * @brief Reads a line from the specified stream into the buffer pointed to by s.
 *
 * @param s Pointer to the buffer where the read line will be stored.
 * @param n Maximum number of characters to read, including the null terminator.
 * @param stream Pointer to a FILE object that identifies the stream.
 * @return On success, the pointer to the buffer s is returned. On end of file or error, NULL is returned.
 */
char *fgets(char *s, int n, FILE *stream);

/**
 * @brief Writes the character c (converted to an unsigned char) to the standard output (stdout).
 *
 * @param c Character to be written.
 * @return On success, the character written is returned. On error, EOF is returned.
 */
int putchar(int c);

/**
 * @brief Writes the character c (converted to an unsigned char) to the stream pointed to by stream.
 *
 * @param c Character to be written.
 * @param stream Pointer to a FILE object that identifies the stream where the character will be written.
 * @return On success, the character written is returned. On error, EOF is returned.
 */
int fputc(int c, FILE *stream);

/**
 * @brief Writes a null-terminated string to the specified stream.
 *
 * @param s Pointer to the null-terminated string to be written.
 * @param stream Pointer to a FILE object that identifies the stream where the string will be written.
 * @return On success, a non-negative number is returned. On error, EOF is returned.
 */
int fputs(const char *s, FILE *stream);

/**
 * @brief Writes a null-terminated string to the standard output (stdout), followed by a newline character.
 *
 * @param str Pointer to the null-terminated string to be written.
 * @return On success, a non-negative number is returned. On error, EOF is returned.
 */
int puts(const char *str);

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

/**
 * @brief Writes a formatted string to the standard output (stdout).
 *
 * @param format C string that contains a format string that follows the same specifications as format in printf.
 * @return On success, the total number of characters written is returned.
 */
int printf(const char *format, ...);

/**
 * @brief Writes a formatted string to the specified output stream.
 *
 * @param stream Pointer to a FILE object that identifies an output stream.
 * @param format C string that contains a format string that follows the same specifications as format in printf.
 * @return On success, the total number of characters written is returned.
 */
int fprintf(FILE *stream, const char *format, ...);

__END_DECLS

#endif