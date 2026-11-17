#ifndef _STDBOOL_H_
#define _STDBOOL_H_

#include <sys/cdefs.h>

__BEGIN_DECLS

#ifndef __cplusplus

#define bool _Bool

#ifndef true
#define true 1
#endif

#ifndef false
#define false 0
#endif

#endif /* __cplusplus */

__END_DECLS

#endif /* _STDBOOL_H_ */