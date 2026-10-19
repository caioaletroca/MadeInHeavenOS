#ifndef _SYS_IOCLT_H_
#define _SYS_IOCLT_H_

#include <sys/cdefs.h>
#include <mihos/ioctl.h>

/**
 * Structure representing the size of a terminal window.
 */
struct winsize
{
    unsigned short ws_row;
    unsigned short ws_col;
    unsigned short ws_xpixel;
    unsigned short ws_ypixel;
};

#define TIOCGWINSZ MIHOS_TIOCGWINSZ

__BEGIN_DECLS

/**
 * @brief Sends a device-specific request to an open file.
 *
 * @param fd The file descriptor.
 * @param request The request (TIOCGWINSZ, ...).
 * @param ... Its argument: usually a pointer to the struct the request reads or fills.
 * @return The request's result (0 for most), or -1 with errno set (EBADF; ENOTTY when the
 *         file has no such request; EFAULT; EINVAL).
 */
int ioctl(int fd, unsigned long request, ...);

__END_DECLS

#endif /* _SYS_IOCLT_H_ */