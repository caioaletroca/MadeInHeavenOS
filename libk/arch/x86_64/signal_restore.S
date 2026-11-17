#include <mihos/syscall.h>

.text
.code64
.globl __signal_restore

/* Where a signal handler returns to: the kernel put this address on its stack */
.type __signal_restore, @function
__signal_restore: 
    mov $SYS_SIGRETURN, %rax
    int $0x80
    ud2
.size __signal_restore, .-__signal_restore
