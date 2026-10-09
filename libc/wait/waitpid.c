#include <sys/wait.h>
#include <syscall.h>
#include <unistd.h>
#include <errno.h>

pid_t waitpid(pid_t pid, int *status, int options)
{
    if (options != 0)
    {
        errno = EINVAL;
        return -1;
    }

    return syscall(SYS_WAIT, pid, status);
}