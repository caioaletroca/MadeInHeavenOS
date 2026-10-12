#ifndef _UAPI_MIHOS_FCNTL_H_
#define _UAPI_MIHOS_FCNTL_H_

// open() flags: the access mode is the low two bits
#define O_RDONLY 0
#define O_WRONLY 1
#define O_RDWR 2
#define O_ACCMODE 3

// lseek() whence
#define SEEK_SET 0 // offset from the start
#define SEEK_CUR 1 // offset from the current position
#define SEEK_END 2 // offset from the end of the file

#endif // _UAPI_MIHOS_FCNTL_H_