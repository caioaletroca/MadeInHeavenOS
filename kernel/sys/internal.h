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

#endif // _SYS_INTERNAL_H_
