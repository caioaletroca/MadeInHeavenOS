#ifndef _SYS_INTERNAL_H_
#define _SYS_INTERNAL_H_

/*
 * What the files of sys/ share, and nothing outside it includes (the rest of
 * the kernel sees include/syscall.h): the helpers that copy from user memory
 * and find an fd, and the sys_* functions syscall.c's dispatch calls. Each
 * sys_* lives in the file of its topic; what only one file uses stays static
 * in it.
 */

#include <syscall.h>
#include <addresses.h>
#include <asm/memory.h>
#include <mm/address_space.h>
#include <mm/kmalloc.h>
#include <sched/scheduler.h>
#include <sched/thread.h>
#include <sched/process.h>
#include <fs/vfs.h>
#include <util.h>

// user_copy.c
address_space_t *syscall_space(void);
long user_string_copy(uintptr_t address, char *buffer, size_t size);
char *user_path_copy(uintptr_t path, long *error);

// files.c
file_t *sys_file(unsigned long fd);
long sys_write(unsigned long fd, uintptr_t buffer, size_t size);
long sys_read(unsigned long fd, uintptr_t buffer, size_t size);
long sys_close(unsigned long fd);
long sys_dup(unsigned long fd);
long sys_dup2(unsigned long oldfd, unsigned long newfd);
long sys_open(uintptr_t path, int flags, uint32_t mode);
long sys_lseek(unsigned long fd, long offset, int whence);
long sys_ioctl(unsigned long fd, unsigned long request, uintptr_t arg);
long sys_ftruncate(unsigned long fd, long length);
long sys_truncate(uintptr_t path, long length);
long sys_pipe(uintptr_t fds);
long sys_fcntl(unsigned long fd, int command, long arg);

// directories.c
long sys_getdents(unsigned long fd, uintptr_t buffer, size_t size);
long sys_unlink(uintptr_t path);
long sys_mkdir(uintptr_t path, uint32_t mode);
long sys_rmdir(uintptr_t path);
long sys_chdir(uintptr_t path);
long sys_getcwd(uintptr_t buffer, size_t size);

// stat.c
long sys_stat(uintptr_t path, uintptr_t statbuf);
long sys_fstat(unsigned long fd, uintptr_t statbuf);

// processes.c
long sys_exit(long status);
long sys_spawn(uintptr_t path, uintptr_t argv, uintptr_t envp);
long sys_fork(void);
long sys_exec(uintptr_t path, uintptr_t argv, uintptr_t envp);
long sys_wait(long pid, uintptr_t status, int options);
long sys_getpid(void);
long sys_getppid(void);
long sys_setpgid(long pid, long group);
long sys_getpgid(long pid);
long sys_setsid(void);
long sys_getsid(long pid);

// signals.c
long sys_kill(long pid, int signal);
long sys_sigaction(int signal, uintptr_t action, uintptr_t old);
long sys_sigprocmask(int how, uintptr_t set, uintptr_t old);
long sys_sigreturn(void);
long sys_sigpending(uintptr_t set);
long sys_pause(void);

// time.c
long sys_clock_gettime(int clock, uintptr_t time);
long sys_nanosleep(uintptr_t request, uintptr_t remaining);

// memory.c
long sys_brk(uintptr_t address);

// power.c
long sys_reboot(long command);

#endif // _SYS_INTERNAL_H_
