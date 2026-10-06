#ifndef _FS_FILE_H_
#define _FS_FILE_H_

#include <stdint.h>
#include <stddef.h>

struct file;

typedef struct file_ops
{
    long (*read)(struct file *file, char *buf, size_t count);
    long (*write)(struct file *file, const char *buf, size_t count);
    void (*release)(struct file *file);
} file_ops_t;

#define FILE_READ (1u << 0)
#define FILE_WRITE (1u << 1)

typedef struct file
{
    const file_ops_t *ops;
    unsigned int flags;
    unsigned int refs;
    void *private;
} file_t;

/**
 * Create a new file object.
 *
 * @param ops The file operations for the new file.
 * @param flags The flags for the new file.
 * @param private The private data associated with the new file.
 * @return A pointer to the newly created file object.
 */
file_t *file_create(const file_ops_t *ops, unsigned int flags, void *private);

/**
 * Increment the reference count of a file object.
 *
 * @param file The file object to increment the reference count for.
 * @return The same file object with an incremented reference count.
 */
file_t *file_get(file_t *file);

/**
 * Decrement the reference count of a file object.
 *
 * @param file The file object to decrement the reference count for.
 */
void file_put(file_t *file);

#endif /* _FS_FILE_H_ */