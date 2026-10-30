#ifndef _UNISTD_H_
#define _UNISTD_H_

#include <sys/cdefs.h>
#include <sys/types.h>
#include <stddef.h>
#include <stdint.h>
#include <mihos/fcntl.h>

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
 * Reposition the read/write file offset of the open file associated with the file descriptor.
 *
 * @param fd The file descriptor of the open file.
 * @param offset The offset to set the file position to.
 * @param whence The reference point for the offset (SEEK_SET, SEEK_CUR, SEEK_END).
 * @return The resulting offset location as measured in bytes from the beginning of the file, or -1 on error.
 */
off_t lseek(int fd, off_t offset, int whence);

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
 * Truncate an open file to a specified length.
 *
 * @param fd The file descriptor of the open file.
 * @param length The new length of the file.
 * @return 0 on success, or -1 on error.
 */
int ftruncate(int fd, off_t length);

/**
 * Truncate a file specified by its path to a given length.
 *
 * @param path The path to the file.
 * @param length The new length of the file.
 * @return 0 on success, or -1 on error.
 */
int truncate(const char *path, off_t length);

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
 * Remove a file specified by its path.
 *
 * @param path The path to the file to remove.
 * @return 0 on success, or -1 on error.
 */
int unlink(const char *path);

/**
 * Remove a directory at the specified path.
 *
 * @param path The path to the directory to remove.
 * @return 0 on success, or -1 on error.
 */
int rmdir(const char *path);

/**
 * Change the current directory: relative paths start there from now on.
 *
 * @param path The new current directory.
 * @return 0 on success, or -1 with errno set (ENOENT, ENOTDIR, ENAMETOOLONG, EFAULT).
 */
int chdir(const char *path);

/**
 * Close a file descriptor.
 *
 * @param fd The file descriptor to close.
 * @return 0 on success, or -1 on error.
 */
int close(int fd);

/**
 * Get the process ID of the calling process.
 *
 * @return The process ID of the calling process.
 */
pid_t getpid(void);

/**
 * Get the parent process ID of the calling process.
 *
 * @return The parent process ID of the calling process.
 */
pid_t getppid(void);

/**
 * Check if a file descriptor refers to a terminal.
 *
 * @param fd The file descriptor to check.
 * @return 1 if the file descriptor refers to a terminal, 0 otherwise.
 */
int isatty(int fd);

/**
 * Terminate the calling process immediately.
 *
 * @param status The exit status of the process.
 */
__attribute__((__noreturn__)) void _exit(int status);

__END_DECLS

#endif