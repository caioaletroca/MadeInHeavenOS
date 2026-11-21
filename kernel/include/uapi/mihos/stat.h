#ifndef _UAPI_MIHOS_STAT_H_
#define _UAPI_MIHOS_STAT_H_

#include <stdint.h>
#include <mihos/time.h>

// st_mode: the file type in the high bits (S_IFMT mask), permissions in the low 12 (octal, Linux values)
#define MIHOS_S_IFMT 0170000
#define MIHOS_S_IFDIR 0040000 // directory
#define MIHOS_S_IFCHR 0020000 // character device (the console)
#define MIHOS_S_IFREG 0100000 // regular file
#define MIHOS_S_IFBLK 0060000 // block device (a disk)
#define MIHOS_S_IFIFO 0010000 // FIFO (a pipe)

// st_rdev: which device a device file is, in Linux's encoding (the minor's low byte in bits 0-7,
// the major's low 12 bits in 8-19, the rest above), so major()/minor() agree with mlibc's
#define MIHOS_MKDEV(major, minor)                                                      \
    ((((uint64_t)(major) & 0xFFFFF000u) << 32) | (((uint64_t)(major) & 0xFFFu) << 8) | \
     (((uint64_t)(minor) & 0xFFFFFF00u) << 12) | ((uint64_t)(minor) & 0xFFu))
#define MIHOS_MAJOR(dev) ((uint32_t)((((dev) >> 32) & 0xFFFFF000u) | (((dev) >> 8) & 0xFFFu)))
#define MIHOS_MINOR(dev) ((uint32_t)((((dev) >> 12) & 0xFFFFFF00u) | ((dev) & 0xFFu)))

// utimensat's special tv_nsec values (Linux values)
#define MIHOS_UTIME_NOW ((1l << 30) - 1l)  // this time becomes now
#define MIHOS_UTIME_OMIT ((1l << 30) - 2l) // this time stays as it is

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