#include <sys/wait.h>
#include <syscall.h>
#include <unistd.h>

pid_t waitpid(pid_t pid, int *status, int options)
{
    return syscall(SYS_WAIT, pid, status, options);
}