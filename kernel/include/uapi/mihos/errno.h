#ifndef _UAPI_MIHOS_ERRNO_H_
#define _UAPI_MIHOS_ERRNO_H_

#define EPERM 1         // operation not permitted
#define ENOENT 2        // no such file: spawn of an unknown path
#define EIO 5           // input/output error
#define E2BIG 7         // argument list too long
#define ENOEXEC 8       // exec format error
#define EBADF 9         // bad file descriptor
#define ECHILD 10       // no such child: wait on a pid that is not ours
#define ENOMEM 12       // out of memory
#define EACCES 13       // permission denied
#define EFAULT 14       // bad address
#define EBUSY 16        // device or resource busy
#define EEXIST 17       // already exists: a device name registered twice
#define ENOTDIR 20      // not a directory
#define EISDIR 21       // is a directory
#define EINVAL 22       // invalid argument: waitpid options
#define EMFILE 24       // too many open files
#define ENOTTY 25       // not a terminal: isatty on a file
#define EFBIG 27        // file too large: a write past the largest file the file system holds
#define ENOSPC 28       // no space left on device: a write at the end of a disk
#define ESPIPE 29       // illegal seek
#define EROFS 30        // read-only file system
#define EMLINK 31       // too many links: a directory with the most subdirectories ext2 allows
#define ENAMETOOLONG 36 // file name too long
#define ENOSYS 38       // function not implemented
#define ENOTEMPTY 39    // directory not empty
#define ENOBUFS 105     // no buffer space: every block cache buffer is in use

#endif /* _UAPI_MIHOS_ERRNO_H_ */