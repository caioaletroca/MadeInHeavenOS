#include <stddef.h>
#include <spawn.h>
#include <syscall.h>
#include <errno.h>

int posix_spawn(pid_t *pid, const char *path, const posix_spawn_file_actions_t *file_actions, const posix_spawnattr_t *attr, char *const argv[], char *const envp[])
{
    if (file_actions != NULL || attr != NULL)
        return ENOSYS;

    long ret = __syscall(SYS_SPAWN, path, argv, envp);
    if (ret < 0)
        return (int)-ret;

    if (pid != NULL)
        *pid = (pid_t)ret;

    return 0;
}