#ifndef _SYS_STAT_H_

#define _SYS_STAT_H_

#include <sys/cdefs.h>
#include <sys/types.h>
#include <time.h>
#include <mihos/stat.h>

// File type (st_mode & S_IFMT)
#define S_IFMT MIHOS_S_IFMT
#define S_IFDIR MIHOS_S_IFDIR
#define S_IFCHR MIHOS_S_IFCHR
#define S_IFREG MIHOS_S_IFREG
#define S_IFBLK MIHOS_S_IFBLK

#define S_ISDIR(m) (((m) & S_IFMT) == S_IFDIR)
#define S_ISCHR(m) (((m) & S_IFMT) == S_IFCHR)
#define S_ISREG(m) (((m) & S_IFMT) == S_IFREG)
#define S_ISBLK(m) (((m) & S_IFMT) == S_IFBLK)

// Permissions: owner, group, others, read/write/execute
#define S_IRWXU 0700
#define S_IRUSR 0400
#define S_IWUSR 0200
#define S_IXUSR 0100
#define S_IRWXG 0070
#define S_IRGRP 0040
#define S_IWGRP 0020
#define S_IXGRP 0010
#define S_IRWXO 0007
#define S_IROTH 0004
#define S_IWOTH 0002
#define S_IXOTH 0001

/**
 * A file's metadata (POSIX).
 */
struct stat
{
    dev_t st_dev;
    ino_t st_ino;
    mode_t st_mode;
    nlink_t st_nlink;
    uid_t st_uid;
    gid_t st_gid;
    dev_t st_rdev;
    off_t st_size;
    blksize_t st_blksize;
    blkcnt_t st_blocks;
    struct timespec st_atim;
    struct timespec st_mtim;
    struct timespec st_ctim;
};

// The pre-2008 names, still what most programs use
#define st_atime st_atim.tv_sec
#define st_mtime st_mtim.tv_sec
#define st_ctime st_ctim.tv_sec

__BEGIN_DECLS

/**
 * @brief Describes the file at a path.
 *
 * @param path The file's path.
 * @param st Receives the description.
 * @return 0, or -1 with errno set (ENOENT, ENOTDIR, ENAMETOOLONG, EFAULT).
 */
int stat(const char *restrict path, struct stat *restrict st);

/**
 * @brief Describes an open file (the console is a character device).
 *
 * @param fd The file descriptor.
 * @param st Receives the description.
 * @return 0, or -1 with errno set (EBADF, EFAULT).
 */
int fstat(int fd, struct stat *st);

/**
 * @brief Create a directory at the specified path.
 *
 * @param path The path to the directory to create.
 * @param mode The permissions for the new directory (07777).
 * @return 0 on success, or -1 with errno set (EEXIST, ENOENT, ENOTDIR, EFAULT).
 */
int mkdir(const char *path, mode_t mode);

__END_DECLS

#endif /* _SYS_STAT_H_ */