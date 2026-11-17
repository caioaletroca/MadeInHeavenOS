#ifndef _FCNTL_H_
#define _FCNTL_H_

#include <sys/cdefs.h>
#include <sys/types.h>
#include <mihos/fcntl.h>

__BEGIN_DECLS

/**
 * @brief Opens the file specified by the path.
 *
 * @param path The path to the file to be opened.
 * @param flags The flags that determine the file access mode and other options.
 * @param ... Optional mode_t argument specifying the file mode (required when creating a new file).
 * @return On success, the file descriptor of the opened file is returned. On error, -1 is returned.
 */
int open(const char *path, int flags, ...);

/**
 * @brief Creates a file, or empties an existing one, and opens it for writing:
 * open(path, O_WRONLY | O_CREAT | O_TRUNC, mode).
 *
 * @param path The path to the file to be created.
 * @param mode The file mode specifying the permissions for the new file.
 * @return On success, the file descriptor of the created file is returned. On error, -1 is returned.
 */
int creat(const char *path, mode_t mode);

/**
 * @brief Reads or sets a file descriptor's flags.
 *
 * @param fd The file descriptor.
 * @param command F_GETFD, or F_SETFD with the new flags as a third argument.
 * @return The flags for F_GETFD, 0 for F_SETFD. On error, -1 is returned.
 */
int fcntl(int fd, int command, ...);

__END_DECLS

#endif /* _FCNTL_H_ */