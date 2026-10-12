#ifndef _INTERNAL_DIRSTREAM_H_
#define _INTERNAL_DIRSTREAM_H_

#include <stdint.h>
#include <stddef.h>
#include <dirent.h>

#define DIRSTREAM_BUFFER_SIZE 2048 // several records; the largest is 272 bytes

/**
 * A directory stream: the fd plus the records of the last getdents.
 */
struct __dirstream
{
    int fd;
    size_t pos;          // offset of the next record in buffer
    size_t len;          // bytes of records in buffer; pos == len: refill
    struct dirent entry; // what readdir returns, overwritten by the next call

    // Records hold a uint64_t at their start, and every record starts at a
    // multiple of 8, so the buffer must be 8-aligned too
    _Alignas(uint64_t) unsigned char buffer[DIRSTREAM_BUFFER_SIZE];
};

#endif // _INTERNAL_DIRSTREAM_H_