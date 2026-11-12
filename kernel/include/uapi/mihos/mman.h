#ifndef _UAPI_MIHOS_MMAN_H_
#define _UAPI_MIHOS_MMAN_H_

// mmap() protection (Linux's values)
#define PROT_NONE 0
#define PROT_READ 1
#define PROT_WRITE 2
#define PROT_EXEC 4

// mmap() flags: exactly one of MAP_SHARED and MAP_PRIVATE
#define MAP_SHARED 0x01
#define MAP_PRIVATE 0x02
#define MAP_FIXED 0x10     // exactly at the address, replacing what is there
#define MAP_ANONYMOUS 0x20 // zeroed memory, no file (fd and offset ignored)
#define MAP_ANON MAP_ANONYMOUS

#endif // _UAPI_MIHOS_MMAN_H_