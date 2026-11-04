#include "internal.h"
#include <exec/exec.h>

// ---- Processes: exit, spawn, wait, pids ----

#define SPAWN_ARGS_MAX 64 // argc and envp entries
#define SPAWN_STRINGS_MAX (4 * 1024)
#define SPAWN_PATH_MAX 64

typedef struct spawn_args
{
    char path[SPAWN_PATH_MAX];
    char *argv[SPAWN_ARGS_MAX + 1];
    char *envp[SPAWN_ARGS_MAX + 1];
    char strings[SPAWN_STRINGS_MAX];
    size_t used;
} spawn_args_t;

static long user_vector_copy(uintptr_t address, char **vector, spawn_args_t *args)
{
    for (size_t i = 0; i <= SPAWN_ARGS_MAX; i++)
    {
        uintptr_t string;
        if (address_space_read(syscall_space(), address + i * sizeof(uintptr_t), &string, sizeof(string)) != 0)
            return -EFAULT;

        if (string == 0)
        {
            vector[i] = NULL;
            return 0;
        }

        if (i == SPAWN_ARGS_MAX)
            return -E2BIG;

        char *copy = &args->strings[args->used];
        long length = user_string_copy(string, copy, SPAWN_STRINGS_MAX - args->used);
        if (length < 0)
            return length;

        vector[i] = copy;
        args->used += (size_t)length + 1;
    }

    return -E2BIG;
}

/**
 * exit(status): terminates the current thread with the given status.
 */
long sys_exit(long status)
{
    process_exit((int)status);
}

/**
 * spawn(path, argv, envp): creates a new process with the specified executable, arguments, and environment.
 */
long sys_spawn(uintptr_t path, uintptr_t argv, uintptr_t envp)
{
    process_t *parent = scheduler_current()->process;

    spawn_args_t *args = kmalloc(sizeof(spawn_args_t));
    if (args == NULL)
        return -ENOMEM;

    args->used = 0;
    long ret = user_string_copy(path, args->path, sizeof(args->path));
    if (ret >= 0)
        ret = user_vector_copy(argv, args->argv, args);
    if (ret >= 0)
    {
        if (envp == 0)
            args->envp[0] = NULL;
        else
            ret = user_vector_copy(envp, args->envp, args);
    }

    void *image = NULL;
    size_t size = 0;
    if (ret >= 0)
    {
        ret = vfs_read_file(args->path, &image, &size);
        if (ret == -EISDIR)
            ret = -EACCES;
    }

    address_space_t *space = NULL;
    uintptr_t entry, stack;
    if (ret >= 0)
        ret = exec_load(image, size, args->argv, args->envp, &space, &entry, &stack);

    kfree(args);
    kfree(image);
    if (ret < 0)
        return ret;

    // From here the child owns the space: releasing it destroys both
    process_t *child = process_create(space);
    if (child == NULL)
    {
        address_space_destroy(space);
        return -ENOMEM;
    }

    process_files_inherit(child, parent);
    process_cwd_inherit(child, parent);

    // The handle from process_create is the parent's reference, held until wait
    process_child_add(parent, child);

    if (process_start(child, entry, stack) < 0)
    {
        process_child_remove(child);
        process_release(child);
        return -ENOMEM;
    }

    return child->pid;
}

/**
 * Encodes the wait status for a process.
 *
 * @param p The process whose status is to be encoded.
 * @return The encoded wait status.
 */
static int wait_status_encode(const process_t *p)
{
    return p->signal != 0 ? p->signal : (p->exit_status & 0xFF) << 8;
}

/**
 * wait(pid, status): waits for child pid, or any child for -1, and retrieves its exit status.
 */
long sys_wait(long pid, uintptr_t status)
{
    process_t *child;
    process_t *parent = scheduler_current()->process;
    long ret = process_wait_child(parent, pid, &child);
    if (ret < 0)
        return ret;

    int encoded = wait_status_encode(child);
    process_release(child);

    if (status != 0 && address_space_write(syscall_space(), status, &encoded, sizeof(encoded)) != 0)
        return -EFAULT;

    return ret;
}

/**
 * Retrieves the process ID of the calling process.
 *
 * @return The process ID of the calling process.
 */
long sys_getpid(void)
{
    process_t *current = scheduler_current()->process;
    return current->pid;
}

/**
 * Retrieves the parent process ID of the calling process.
 *
 * @return The parent process ID of the calling process.
 */
long sys_getppid(void)
{
    process_t *current = scheduler_current()->process;
    return process_parent_pid(current);
}
