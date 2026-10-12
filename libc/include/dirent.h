#ifndef _DIRENT_H_
#define _DIRENT_H_

#include <stdint.h>
#include <stddef.h>
#include <sys/cdefs.h>
#include <sys/types.h>
#include <mihos/dirent.h>

// d_type values: the kernel's record types (BSD/Linux numbers)
#define DT_UNKNOWN 0
#define DT_DIR MIHOS_DT_DIR
#define DT_REG MIHOS_DT_REG

// Opaque directory stream, like FILE: only libc knows what is inside
typedef struct __dirstream DIR;

/**
 * Directory entry structure.
 */
struct dirent
{
    ino_t d_ino;
    unsigned char d_type;
    char d_name[256];
};

__BEGIN_DECLS

/**
 * @brief Opens a directory stream.
 *
 * @param path The directory's path.
 * @return The stream, or NULL with errno set (ENOENT, ENOTDIR for a file, ENOMEM, ...).
 */
DIR *opendir(const char *path);

/**
 * @brief Reads the next entry of a directory stream ("." and ".." included).
 *
 * The result points into the stream and is overwritten by the next readdir on it.
 *
 * @param dir The stream.
 * @return The entry; NULL at the end (errno unchanged) or on an error (errno set).
 */
struct dirent *readdir(DIR *dir);

/**
 * @brief Moves a directory stream back to its first entry.
 *
 * @param dir The stream.
 */
void rewinddir(DIR *dir);

/**
 * @brief Closes a directory stream and frees it.
 *
 * @param dir The stream.
 * @return 0 on success, -1 with errno set on error.
 */
int closedir(DIR *dir);

/**
 * @brief Reads raw directory records (struct mihos_dirent) from a directory's fd.
 *
 * Not POSIX (Linux has getdents64); readdir is built on it.
 *
 * @param fd An open directory.
 * @param buffer Where the records go.
 * @param size The buffer's size.
 * @return Bytes written, 0 at the end, or -1 with errno set (EINVAL if not even one record fits).
 */
ssize_t getdents(int fd, void *buffer, size_t size);

__END_DECLS

#endif // _DIRENT_H_