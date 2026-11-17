#ifndef _INTERNAL_STAT_H_
#define _INTERNAL_STAT_H_

#include <sys/stat.h>

/**
 * Copies the kernel's struct mihos_stat into POSIX's struct stat, field by field.
 */
static inline void __stat_from_kernel(struct stat *st, const struct mihos_stat *k)
{
    st->st_dev = k->st_dev;
    st->st_ino = k->st_ino;
    st->st_mode = k->st_mode;
    st->st_nlink = k->st_nlink;
    st->st_uid = k->st_uid;
    st->st_gid = k->st_gid;
    st->st_rdev = k->st_rdev;
    st->st_size = k->st_size;
    st->st_blksize = k->st_blksize;
    st->st_blocks = k->st_blocks;
    st->st_atim.tv_sec = k->st_atim.tv_sec;
    st->st_atim.tv_nsec = k->st_atim.tv_nsec;
    st->st_mtim.tv_sec = k->st_mtim.tv_sec;
    st->st_mtim.tv_nsec = k->st_mtim.tv_nsec;
    st->st_ctim.tv_sec = k->st_ctim.tv_sec;
    st->st_ctim.tv_nsec = k->st_ctim.tv_nsec;
}

#endif /* _INTERNAL_STAT_H_ */