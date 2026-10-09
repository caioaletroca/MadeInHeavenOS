#ifndef _UAPI_MIHOS_ERRNO_H_
#define _UAPI_MIHOS_ERRNO_H_

#define ENOENT 2 // no such file: spawn of an unknown path
#define E2BIG 7  // argument list too long
#define ENOEXEC 8
#define EBADF 9
#define ECHILD 10 // no such child: wait on a pid that is not ours
#define ENOMEM 12
#define EFAULT 14
#define EINVAL 22 // invalid argument: waitpid options
#define EMFILE 24
#define ENOSYS 38

#endif /* _UAPI_MIHOS_ERRNO_H_ */