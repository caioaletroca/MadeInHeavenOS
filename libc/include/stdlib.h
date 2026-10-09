#ifndef _STDLIB_H_
#define _STDLIB_H_

#include <stddef.h>
#include <sys/cdefs.h>

__BEGIN_DECLS

/**
 * @brief Aborts the program.
 *
 * This function causes abnormal program termination.
 */
__attribute__((__noreturn__)) void abort(void);

int atexit(void (*)(void));

/**
 * @brief Retrieves the value of an environment variable.
 *
 * @param name The name of the environment variable.
 * @return A pointer to the value of the environment variable, or NULL if it does not exist.
 */
char *getenv(const char *name);

/**
 * @brief Allocates a block of memory of the specified size.
 *
 * @param size The size of the memory block to allocate.
 * @return A pointer to the allocated memory, or NULL if the allocation fails.
 */
void *malloc(size_t size);

/**
 * @brief Allocates a block of memory for an array of elements, initializing all bytes to zero.
 *
 * @param number The number of elements.
 * @param size The size of each element.
 * @return A pointer to the allocated memory, or NULL if the allocation fails.
 */
void *calloc(size_t number, size_t size);

/**
 * @brief Reallocates a block of memory to a new size.
 *
 * @param ptr Pointer to the previously allocated memory block.
 * @param size The new size of the memory block.
 * @return A pointer to the reallocated memory, or NULL if the reallocation fails.
 */
void *realloc(void *ptr, size_t size);

/**
 * @brief Frees a previously allocated block of memory.
 *
 * @param ptr Pointer to the memory block to free.
 */
void free(void *ptr);

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

/**
 * @brief Terminates the program with the given status.
 *
 * @param status The exit status.
 */
__attribute__((__noreturn__)) void exit(int status);

__END_DECLS

#endif