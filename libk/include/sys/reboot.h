#ifndef _SYS_REBOOT_H_
#define _SYS_REBOOT_H_

#include <sys/cdefs.h>
#include <mihos/syscall.h>

__BEGIN_DECLS

#define RB_HALT_SYSTEM REBOOT_HALT

/**
 * reboot(howto): reboots or halts the system based on the specified howto flag.
 *
 * @param howto specifies the action to be taken (e.g., RB_HALT_SYSTEM to halt the system).
 * @return 0 on success, -1 on error (errno is set appropriately)
 */
int reboot(int howto);

__END_DECLS

#endif /* _SYS_REBOOT_H_ */