#ifndef _STDBOOL_H_
#define _STDBOOL_H_

#include <sys/cdefs.h>

__BEGIN_DECLS

#define bool _Bool

#ifndef true
    #define true 1
#endif

#ifndef false
    #define false 0
#endif

__END_DECLS

#endif