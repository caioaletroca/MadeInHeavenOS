#ifndef _STDLIB_H_
#define _STDLIB_H_

#include <stddef.h>
#include <sys/cdefs.h>

__BEGIN_DECLS

__attribute__((__noreturn__))
void abort(void);
int atexit(void (*)(void));
void free(void *);
char *getenv(const char *);
void *malloc(size_t size);

/**
 * @brief Performs a absolute operation into a int value
*/
int abs(int i);

/**
 * @brief Converts a string of numbers into an integer.
 * 
 * @return int 
 */
int atoi(const char *str);

__END_DECLS

#endif