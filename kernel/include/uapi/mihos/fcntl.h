#ifndef _UAPI_MIHOS_FCNTL_H_
#define _UAPI_MIHOS_FCNTL_H_

// open() flags: the access mode is the low two bits
#define O_RDONLY 0
#define O_WRONLY 1
#define O_RDWR 2
#define O_ACCMODE 3
#define O_CREAT 0x40  // open(): create the file if it does not exist (Linux's 0100)
#define O_EXCL 0x80   // with O_CREAT: fail with EEXIST if it does (Linux's 0200)
#define O_TRUNC 0x200 // open(): truncate a regular file opened for writing to 0 (Linux's 01000)
#define O_DIRECTORY 0x10000

// lseek() whence
#define SEEK_SET 0 // offset from the start
#define SEEK_CUR 1 // offset from the current position
#define SEEK_END 2 // offset from the end of the file

#endif // _UAPI_MIHOS_FCNTL_H_