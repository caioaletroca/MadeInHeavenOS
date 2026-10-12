#ifndef _UAPI_MIHOS_SYSCALL_H_
#define _UAPI_MIHOS_SYSCALL_H_

// System call numbers for MadeInHeavenOS
#define SYS_EXIT 0
#define SYS_WRITE 1
#define SYS_READ 2
#define SYS_FORK 3
#define SYS_EXEC 4
#define SYS_WAIT 5
#define SYS_GETPID 6
#define SYS_KILL 7
#define SYS_YIELD 8
#define SYS_SLEEP 9
#define SYS_CLOSE 10
#define SYS_BRK 11
#define SYS_SPAWN 12
#define SYS_REBOOT 13
#define SYS_GETPPID 14
#define SYS_OPEN 15
#define SYS_LSEEK 16
#define SYS_GETDENTS 17

// SYS_REBOOT commands
#define REBOOT_HALT 0 // stop the CPU (no ACPI power off yet)

#endif /* _UAPI_MIHOS_SYSCALL_H_ */