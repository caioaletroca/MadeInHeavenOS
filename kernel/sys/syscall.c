#include "internal.h"

// ---- The dispatch: a syscall number to its sys_* function ----

long syscall_dispatch(unsigned long number, unsigned long arg0, unsigned long arg1, unsigned long arg2, unsigned long arg3, unsigned long arg4, unsigned long arg5)
{
    (void)arg3;
    (void)arg4;
    (void)arg5;

    switch (number)
    {
    case SYS_EXIT:
        return sys_exit((long)arg0);
    case SYS_BRK:
        return sys_brk((uintptr_t)arg0);
    case SYS_FORK:
        return sys_fork();
    case SYS_WRITE:
        return sys_write(arg0, arg1, arg2);
    case SYS_CLOSE:
        return sys_close(arg0);
    case SYS_READ:
        return sys_read(arg0, arg1, arg2);
    case SYS_SPAWN:
        return sys_spawn(arg0, arg1, arg2);
    case SYS_WAIT:
        return sys_wait((long)arg0, arg1);
    case SYS_REBOOT:
        return sys_reboot((long)arg0);
    case SYS_GETPID:
        return sys_getpid();
    case SYS_GETPPID:
        return sys_getppid();
    case SYS_OPEN:
        return sys_open(arg0, (int)arg1, (uint32_t)arg2);
    case SYS_LSEEK:
        return sys_lseek((unsigned long)arg0, (long)arg1, (int)arg2);
    case SYS_GETDENTS:
        return sys_getdents(arg0, (uintptr_t)arg1, (size_t)arg2);
    case SYS_STAT:
        return sys_stat((uintptr_t)arg0, (uintptr_t)arg1);
    case SYS_FSTAT:
        return sys_fstat((unsigned long)arg0, (uintptr_t)arg1);
    case SYS_IOCTL:
        return sys_ioctl((unsigned long)arg0, (unsigned long)arg1, (uintptr_t)arg2);
    case SYS_FTRUNCATE:
        return sys_ftruncate((unsigned long)arg0, (long)arg1);
    case SYS_TRUNCATE:
        return sys_truncate(arg0, (long)arg1);
    case SYS_UNLINK:
        return sys_unlink((uintptr_t)arg0);
    case SYS_MKDIR:
        return sys_mkdir((uintptr_t)arg0, (uint32_t)arg1);
    case SYS_RMDIR:
        return sys_rmdir((uintptr_t)arg0);
    case SYS_CHDIR:
        return sys_chdir((uintptr_t)arg0);
    case SYS_GETCWD:
        return sys_getcwd((uintptr_t)arg0, (size_t)arg1);
    case SYS_DUP:
        return sys_dup((unsigned long)arg0);
    case SYS_DUP2:
        return sys_dup2((unsigned long)arg0, (unsigned long)arg1);
    case SYS_PIPE:
        return sys_pipe((uintptr_t)arg0);
    case SYS_FCNTL:
        return sys_fcntl(arg0, (int)arg1, (long)arg2);
    case SYS_EXEC:
        return sys_exec(arg0, arg1, arg2);
    default:
        return -ENOSYS;
    }
}