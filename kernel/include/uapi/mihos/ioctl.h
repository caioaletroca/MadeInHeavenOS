#ifndef _UAPI_MIHOS_IOCTL_H_
#define _UAPI_MIHOS_IOCTL_H_

#include <stdint.h>

/*
 * A request number carries what the syscall layer needs to copy its argument
 * (Linux's asm-generic _IOC layout):
 *   bits 31-30 direction, 29-16 argument size, 15-8 type (a letter per driver), 7-0 number
 */
#define MIHOS_IOC_NONE 0u
#define MIHOS_IOC_WRITE 1u // the caller's argument is copied in (user → kernel)
#define MIHOS_IOC_READ 2u  // the result is copied out (kernel → user)

#define MIHOS_IOC(dir, type, nr, size) \
    (((uint32_t)(dir) << 30) | ((uint32_t)(size) << 16) | ((uint32_t)(type) << 8) | (uint32_t)(nr))
#define MIHOS_IOR(type, nr, arg_type) MIHOS_IOC(MIHOS_IOC_READ, type, nr, sizeof(arg_type))
#define MIHOS_IOW(type, nr, arg_type) MIHOS_IOC(MIHOS_IOC_WRITE, type, nr, sizeof(arg_type))

#define MIHOS_IOC_DIR(request) (((request) >> 30) & 3u)
#define MIHOS_IOC_SIZE(request) (((request) >> 16) & 0x3FFFu)

struct mihos_winsize
{
    uint16_t ws_row;
    uint16_t ws_col;
    uint16_t ws_xpixel;
    uint16_t ws_ypixel;
};

#define MIHOS_TIOCGWINSZ MIHOS_IOR('T', 0x13, struct mihos_winsize) // get the window size
#define MIHOS_TIOCSCTTY MIHOS_IOC(MIHOS_IOC_NONE, 'T', 0x0E, 0)     // make this terminal the session's
#define MIHOS_TIOCGPGRP MIHOS_IOR('T', 0x0F, int32_t)               // get the foreground group
#define MIHOS_TIOCSPGRP MIHOS_IOW('T', 0x10, int32_t)               // set the foreground group
#define MIHOS_TIOCSTI MIHOS_IOW('T', 0x12, char)                    // push a byte into the input

#endif /* _UAPI_MIHOS_IOCTL_H_ */