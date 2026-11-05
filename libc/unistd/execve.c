#include <unistd.h>
#include <syscall.h>

int execve(const char *path, char *const argv[], char *const envp[])
{
    return syscall(SYS_EXEC, path, argv, envp);
}