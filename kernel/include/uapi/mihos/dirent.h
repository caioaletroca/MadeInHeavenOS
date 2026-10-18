#ifndef _UAPI_MIHOS_DIRENT_H_
#define _UAPI_MIHOS_DIRENT_H_

#include <stdint.h>

#define MIHOS_DT_CHR 2
#define MIHOS_DT_DIR 4
#define MIHOS_DT_REG 8

struct mihos_dirent
{
    uint64_t d_ino;
    uint16_t d_reclen;
    uint8_t d_type;
    char d_name[];
};

#endif // _UAPI_MIHOS_DIRENT_H_