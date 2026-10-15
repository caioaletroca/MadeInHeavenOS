#ifndef _UAPI_MIHOS_STAT_H_
#define _UAPI_MIHOS_STAT_H_

#include <stdint.h>

// st_mode: the file type in the high bits (S_IFMT mask), permissions in the low 12 (octal, Linux values)
#define MIHOS_S_IFMT 0170000
#define MIHOS_S_IFDIR 0040000 // directory
#define MIHOS_S_IFCHR 0020000 // character device (the console)
#define MIHOS_S_IFREG 0100000 // regular file

struct mihos_timespec
{
    int64_t tv_sec;  // seconds
    int64_t tv_nsec; // nanoseconds
};

/**
 * What stat and fstat write. Every field is fixed-width and the padding is
 * explicit, so the layout is the same for every compiler (it matches Linux's
 * x86-64 struct stat field for field).
 */
struct mihos_stat
{
    uint64_t st_dev;   // file system the file is on (0: the root; mounts later)
    uint64_t st_ino;   // inode number, unique within st_dev
    uint64_t st_nlink; // names pointing to the file
    uint32_t st_mode;  // MIHOS_S_IF* | permission bits
    uint32_t st_uid;   // owner (0: no users yet)
    uint32_t st_gid;
    uint32_t __pad0;               // keeps st_rdev 8-aligned without hidden padding
    uint64_t st_rdev;              // for devices: which device
    int64_t st_size;               // bytes
    int64_t st_blksize;            // preferred I/O size
    int64_t st_blocks;             // 512-byte units used
    struct mihos_timespec st_atim; // last access
    struct mihos_timespec st_mtim; // last change of the contents
    struct mihos_timespec st_ctim; // last change of the metadata
};

#endif /* _UAPI_MIHOS_STAT_H_ */