#ifndef _SYS_SYSMACROS_H_
#define _SYS_SYSMACROS_H_

#include <mihos/stat.h>

#define makedev(major, minor) MIHOS_MKDEV(major, minor)
#define major(dev) MIHOS_MAJOR(dev)
#define minor(dev) MIHOS_MINOR(dev)

#endif // _SYS_SYSMACROS_H_