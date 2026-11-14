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
// 9: free (a sleep that was never wired up); numbers are never reused
#define SYS_CLOSE 10
#define SYS_BRK 11
#define SYS_SPAWN 12
#define SYS_REBOOT 13
#define SYS_GETPPID 14
#define SYS_OPEN 15
#define SYS_LSEEK 16
#define SYS_GETDENTS 17
#define SYS_STAT 18
#define SYS_FSTAT 19
#define SYS_IOCTL 20
#define SYS_FTRUNCATE 21
#define SYS_TRUNCATE 22
#define SYS_UNLINK 23
#define SYS_MKDIR 24
#define SYS_RMDIR 25
#define SYS_CHDIR 26
#define SYS_GETCWD 27
#define SYS_DUP 28
#define SYS_DUP2 29
#define SYS_PIPE 30
#define SYS_FCNTL 31
#define SYS_SIGACTION 32
#define SYS_SIGPROCMASK 33
#define SYS_SIGRETURN 34
#define SYS_SIGPENDING 35
#define SYS_PAUSE 36
#define SYS_SETPGID 37
#define SYS_GETPGID 38
#define SYS_SETSID 39
#define SYS_GETSID 40
#define SYS_CLOCK_GETTIME 41
#define SYS_NANOSLEEP 42
#define SYS_MMAP 43
#define SYS_MUNMAP 44
#define SYS_MPROTECT 45

// SYS_REBOOT commands
#define REBOOT_HALT 0 // stop the CPU (no ACPI power off yet)

#endif /* _UAPI_MIHOS_SYSCALL_H_ */