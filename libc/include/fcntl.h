#ifndef _FCNTL_H_
#define _FCNTL_H_

#include <sys/cdefs.h>
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

__END_DECLS

#endif /* _FCNTL_H_ */