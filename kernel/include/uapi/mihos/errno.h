#ifndef _UAPI_MIHOS_ERRNO_H_
#define _UAPI_MIHOS_ERRNO_H_

#define EPERM 1  // operation not permitted
#define ENOENT 2 // no such file: spawn of an unknown path
#define EIO 5    // input/output error
#define E2BIG 7  // argument list too long
#define ENOEXEC 8
#define EBADF 9
#define ECHILD 10 // no such child: wait on a pid that is not ours
#define ENOMEM 12
#define EACCES 13 // permission denied
#define EFAULT 14
#define EBUSY 16   // device or resource busy
#define ENOTDIR 20 // not a directory
#define EISDIR 21  // is a directory
#define EINVAL 22  // invalid argument: waitpid options
#define EMFILE 24
#define ENOTTY 25       // not a terminal: isatty on a file
#define ESPIPE 29       // illegal seek
#define EROFS 30        // read-only file system
#define ENAMETOOLONG 36 // file name too long
#define ENOSYS 38

#endif /* _UAPI_MIHOS_ERRNO_H_ */