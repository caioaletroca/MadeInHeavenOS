#ifndef _UAPI_MIHOS_FCNTL_H_
#define _UAPI_MIHOS_FCNTL_H_

// open() flags: the access mode is the low two bits
#define O_RDONLY 0
#define O_WRONLY 1
#define O_RDWR 2
#define O_ACCMODE 3
#define O_CREAT 0x40   // open(): create the file if it does not exist (Linux's 0100)
#define O_EXCL 0x80    // with O_CREAT: fail with EEXIST if it does (Linux's 0200)
#define O_TRUNC 0x200  // open(): truncate a regular file opened for writing to 0 (Linux's 01000)
#define O_APPEND 0x400 // open(): every write goes to the end of the file (Linux's 02000)
#define O_DIRECTORY 0x10000
#define O_CLOEXEC 0x80000 // open(): the new fd is closed by a successful exec (Linux's 02000000)

// lseek() whence
#define SEEK_SET 0 // offset from the start
#define SEEK_CUR 1 // offset from the current position
#define SEEK_END 2 // offset from the end of the file

// fcntl() commands
#define F_DUPFD 0            // duplicate to the lowest free fd >= arg
#define F_GETFD 1            // read the fd's flags
#define F_SETFD 2            // set the fd's flags
#define F_GETFL 3            // read the open file's access mode and status flags
#define F_SETFL 4            // set the open file's status flags (O_APPEND)
#define F_DUPFD_CLOEXEC 1030 // F_DUPFD with FD_CLOEXEC set (Linux's 1024 + 6)

// fd flags
#define FD_CLOEXEC 1 // close this fd in a successful exec

// The *at calls' directory and flags (Linux values)
#define AT_FDCWD -100             // dirfd: the path is from the working directory
#define AT_SYMLINK_NOFOLLOW 0x100 // act on a symlink itself, not what it points at
#define AT_EMPTY_PATH 0x1000      // an empty path means dirfd's own file

#endif // _UAPI_MIHOS_FCNTL_H_