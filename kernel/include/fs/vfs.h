#ifndef _FS_VFS_H_
#define _FS_VFS_H_

#include <stddef.h>
#include <stdint.h>
#include <sched/spinlock.h>

#define VFS_NAME_MAX 255
#define VFS_PATH_MAX 4096

struct file;
struct mihos_stat;

/**
 * @brief Defines the types of nodes in the virtual file system.
 */
typedef enum
{
    VFS_FILE,
    VFS_DIRECTORY,
    VFS_CHARDEV,
    VFS_BLOCKDEV, // a disk: read at byte offsets through the node's ops, seekable
} vfs_node_type_t;

struct file_ops;
struct vfs_node_ops;

/**
 * @brief Represents a node in the virtual file system.
 */
typedef struct vfs_node
{
    vfs_node_type_t type;
    uint64_t size;
    uint64_t ino;                      // inode number, unique within the file system
    uint32_t mode;                     // permission bits only (07777); the type is `type`
    int64_t mtime;                     // last change, seconds since 1970 (0: unknown)
    uint64_t dev;                      // file system id (vfs_dev_alloc), st_dev
    struct vfs_node *mounted;          // mount point: the root of the file system mounted here, or NULL
    struct vfs_node *covers;           // mounted root: the directory it is mounted on, or NULL
    uint64_t rdev;                     // device nodes (char and block): MIHOS_MKDEV(major, minor), st_rdev
    const struct file_ops *device_ops; // character devices: the driver's ops, given to every open file
    const struct vfs_node_ops *ops;
    struct vfs_node *parent;
    unsigned int refs;
    spinlock_t lock;
} vfs_node_t;

/**
 * @brief Represents a directory entry in the virtual file system.
 */
typedef struct vfs_dirent
{
    uint64_t ino;
    vfs_node_type_t type;
    size_t length;
    char name[VFS_NAME_MAX + 1];
} vfs_dirent_t;

/**
 * What a file system implements. Every function receives its own nodes.
 */
typedef struct vfs_node_ops
{
    // Directory: the child called name[0..length) (not '\0'-terminated), as a new reference
    int (*lookup)(vfs_node_t *dir, const char *name, size_t length, vfs_node_t **child);

    // File: copies up to count bytes from offset; returns the count (0 at the end) or -errno
    long (*read)(vfs_node_t *file, void *buffer, size_t count, uint64_t offset);

    // Directory: entry number index into *entry; 1 if there is one, 0 past the last, or -errno
    int (*readdir)(vfs_node_t *dir, uint64_t index, vfs_dirent_t *entry);

    // Called after the last reference is dropped; NULL: the node is never freed
    void (*release)(vfs_node_t *node);
} vfs_node_ops_t;

/**
 * @brief Initialize a VFS node.
 *
 * @param node The node to initialize.
 * @param type The type of the node.
 * @param ops The operations associated with the node.
 */
void vfs_node_init(vfs_node_t *node, vfs_node_type_t type, const vfs_node_ops_t *ops);

/**
 * @brief Increment the reference count of a VFS node.
 *
 * @param node The node to increment the reference count for.
 */
void vfs_node_get(vfs_node_t *node);

/**
 * @brief Decrement the reference count of a VFS node.
 *
 * @param node The node to decrement the reference count for.
 */
void vfs_node_put(vfs_node_t *node);

/**
 * Get the next component of a path.
 *
 * @param path Pointer to the path string.
 * @param length Pointer to store the length of the next component.
 * @return Pointer to the start of the next component.
 */
const char *vfs_path_next(const char **path, size_t *length);

/**
 * @brief Mount the root of the virtual file system.
 *
 * @param root The root node to mount.
 */
void vfs_mount_root(vfs_node_t *root);

/**
 * @brief Allocate a new device ID for a file system.
 *
 * @return A unique device ID, never 0, boot time only (no lock)
 */
uint64_t vfs_dev_alloc(void);

/**
 * @brief Mount a file system at the specified path.
 *
 * @param path The path where the file system should be mounted.
 * @param root The root node of the file system to mount.
 * @return 0 on success, -errno on failure.
 */
int vfs_mount(const char *path, vfs_node_t *root);

/**
 * @brief Look up a node by its path.
 *
 * @param path The path to look up.
 * @param node Pointer to store the resulting node.
 * @return 0 on success, -errno on failure.
 */
int vfs_lookup(const char *path, vfs_node_t **node);

/**
 * @brief Read data from a node.
 *
 * @param node The node to read from.
 * @param buffer The buffer to store the read data.
 * @param count The number of bytes to read.
 * @param offset The offset within the node to start reading from.
 * @return The number of bytes read on success, -errno on failure.
 */
long vfs_read(vfs_node_t *node, void *buffer, size_t count, uint64_t offset);

/**
 * @brief Read a directory entry at a given index.
 *
 * @param dir The directory node to read from.
 * @param index The index of the directory entry to read.
 * @param entry Pointer to store the resulting directory entry.
 * @return 1 if an entry is read, 0 if past the last entry, -errno on failure.
 */
int vfs_readdir(vfs_node_t *dir, uint64_t index, vfs_dirent_t *entry);

/**
 * @brief Get the status of a VFS node.
 *
 * @param node The node to get the status of.
 * @param stat Pointer to store the resulting status.
 */
void vfs_stat(vfs_node_t *node, struct mihos_stat *stat);

/**
 * @brief Fill a buffer with directory records (struct mihos_dirent), starting at the file's offset.
 *
 * The caller holds file->position_lock and stores *next in file->offset once the
 * records reached their destination, so a failed copy loses no entries.
 *
 * @param file An open directory.
 * @param buffer Where the records go (zeroed by the caller: padding is copied out too).
 * @param size The buffer's size.
 * @param next Receives the index of the first entry not returned.
 * @return Bytes written, 0 at the end, -EINVAL if the first record does not fit, or -errno.
 */
long vfs_file_getdents(struct file *file, void *buffer, size_t size, uint64_t *next);

/**
 * @brief Read the entire contents of a file into memory.
 *
 * @param path The path to the file.
 * @param data Pointer to store the allocated buffer containing the file data.
 * @param size Pointer to store the size of the file data.
 * @return 0 on success, -errno on failure.
 */
int vfs_read_file(const char *path, void **data, size_t *size);

/**
 * @brief Open a file by its path.
 *
 * @param path The path to the file.
 * @param flags The flags for opening the file.
 * @param file Pointer to store the resulting file object.
 * @return 0 on success, -errno on failure.
 */
int vfs_open(const char *path, int flags, struct file **file);

#endif /* _FS_VFS_H_ */