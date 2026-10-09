#ifndef _UNISTD_H_
#define _UNISTD_H_

#include <sys/cdefs.h>
#include <sys/types.h>
#include <stddef.h>
#include <stdint.h>

__BEGIN_DECLS

/**
 * The environment vector for the calling process.
 */
extern char **environ;

int execv(const char *, char *const[]);
int execve(const char *, char *const[], char *const[]);
int execvp(const char *, char *const[]);
pid_t fork(void);

/**
 * Set the end of the data segment (heap) for the calling process.
 *
 * @param addr The new end of the data segment.
 * @return 0 on success, or -1 on error.
 */
int brk(void *addr);

/**
 * Increment the program's data space (heap) by a specified amount.
 *
 * @param increment The number of bytes to increase the data segment by.
 * @return The previous end of the data segment on success, or (void *)-1 on error.
 */
void *sbrk(intptr_t increment);

/**
 * Write data to a file descriptor.
 *
 * @param fd The file descriptor to write to.
 * @param buf The buffer containing the data to write.
 * @param count The number of bytes to write.
 * @return The number of bytes written, or -1 on error.
 */
ssize_t write(int fd, const void *buf, size_t count);

/**
 * Read data from a file descriptor.
 *
 * @param fd The file descriptor to read from.
 * @param buf The buffer to store the read data.
 * @param count The number of bytes to read.
 * @return The number of bytes read, or -1 on error.
 */
ssize_t read(int fd, void *buf, size_t count);

/**
 * Close a file descriptor.
 *
 * @param fd The file descriptor to close.
 * @return 0 on success, or -1 on error.
 */
int close(int fd);

/**
 * Terminate the calling process immediately.
 *
 * @param status The exit status of the process.
 */
_Noreturn void _exit(int status);

__END_DECLS

#endif