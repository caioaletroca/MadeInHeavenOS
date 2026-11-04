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

#endif // _SYS_INTERNAL_H_
