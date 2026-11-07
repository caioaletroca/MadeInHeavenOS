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
 * Create a new process by duplicating the calling process.
 *
 * @return The process ID of the newly created child process, or -1 on error.
 */
pid_t fork(void);

/**
 * Execute a program, replacing the current process image.
 *
 * @param path The path to the program to execute.
 * @param argv The argument vector for the program.
 * @return -1 on error.
 */
int execv(const char *path, char *const argv[]);

/**
 * Execute a program, replacing the current process image.
 *
 * @param path The path to the program to execute.
 * @param argv The argument vector for the program.
 * @param envp The environment vector for the program.
 * @return -1 on error.
 */
int execve(const char *path, char *const argv[], char *const envp[]);

/**
 * Execute a program, searching for it in the directories listed in the PATH environment variable.
 *
 * @param name The name of the program to execute.
 * @param argv The argument vector for the program.
 * @return -1 on error.
 */
int execvp(const char *name, char *const argv[]);

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
 * The current directory's absolute path.
 *
 * @param buffer Receives the path, '\0'-terminated.
 * @param size The buffer's size.
 * @return buffer, or NULL with errno set (EINVAL for size 0, ERANGE if the path does not
 *         fit, ENOENT if the directory was removed, EFAULT).
 */
char *getcwd(char *buffer, size_t size);

/**
 * Close a file descriptor.
 *
 * @param fd The file descriptor to close.
 * @return 0 on success, or -1 on error.
 */
int close(int fd);

/**
 * Duplicate a file descriptor.
 *
 * @param fd The file descriptor to duplicate.
 * @return The new file descriptor, or -1 on error.
 */
int dup(int fd);

/**
 * Duplicate a file descriptor to a specified new descriptor.
 *
 * @param oldfd The file descriptor to duplicate.
 * @param newfd The new file descriptor.
 * @return The new file descriptor, or -1 on error.
 */
int dup2(int oldfd, int newfd);

/**
 * Suspend the calling process until a signal is received.
 *
 * @return -1 on error with errno set appropriately.
 */
int pause(void);

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
 * Move a process to a process group.
 *
 * @param pid The process, 0 for the caller.
 * @param group The group, 0 for the one named after the process.
 * @return 0 on success, -1 on error with errno set appropriately.
 */
int setpgid(pid_t pid, pid_t group);

/**
 * Get a process's group.
 *
 * @param pid The process, 0 for the caller.
 * @return The group, or -1 on error with errno set appropriately.
 */
pid_t getpgid(pid_t pid);

/**
 * Get the calling process's group.
 *
 * @return The group.
 */
pid_t getpgrp(void);

/**
 * Make the caller the leader of a new session and of a new group inside it.
 *
 * @return The new session, or -1 on error with errno set appropriately.
 */
pid_t setsid(void);

/**
 * Get a process's session.
 *
 * @param pid The process, 0 for the caller.
 * @return The session, or -1 on error with errno set appropriately.
 */
pid_t getsid(pid_t pid);

/**
 * Check if a file descriptor refers to a terminal.
 *
 * @param fd The file descriptor to check.
 * @return 1 if the file descriptor refers to a terminal, 0 otherwise.
 */
int isatty(int fd);

/**
 * Create a pipe and return file descriptors for the read and write ends.
 *
 * @param fds An array of two integers where the file descriptors will be stored.
 * @return 0 on success, or -1 on error.
 */
int pipe(int fds[2]);

/**
 * Terminate the calling process immediately.
 *
 * @param status The exit status of the process.
 */
__attribute__((__noreturn__)) void _exit(int status);

__END_DECLS

#endif