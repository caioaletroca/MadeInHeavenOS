#include "internal.h"
#include <exec/exec.h>
#include <sched/signal.h>

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
 * Create a child process with the given parent and address space.
 *
 * @param parent The parent process.
 * @param space The address space for the child process.
 * @return A pointer to the newly created child process, or NULL on failure.
 */
static process_t *child_create(process_t *parent, address_space_t *space)
{
    // From here the child owns the space: releasing it destroys both
    process_t *child = process_create(space);
    if (child == NULL)
    {
        address_space_destroy(space);
        return NULL;
    }

    process_files_inherit(child, parent);
    process_cwd_inherit(child, parent);
    signal_inherit(child, parent);

    // The handle from process_create is the parent's reference, held until wait
    process_child_add(parent, child);

    return child;
}

/**
 * Load a program into a new address space.
 *
 * @param path The path to the executable.
 * @param argv The argument vector.
 * @param envp The environment vector.
 * @param space Pointer to store the new address space.
 * @param entry Pointer to store the entry point of the program.
 * @param stack Pointer to store the initial stack pointer.
 * @return 0 on success, or a negative error code on failure.
 */
static long program_load(uintptr_t path, uintptr_t argv, uintptr_t envp, address_space_t **space, uintptr_t *entry, uintptr_t *stack)
{
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

    if (ret >= 0)
        ret = exec_load(image, size, args->argv, args->envp, space, entry, stack);

    kfree(args);
    kfree(image);
    return ret < 0 ? ret : 0;
}

/**
 * spawn(path, argv, envp): creates a new process with the specified executable, arguments, and environment.
 */
long sys_spawn(uintptr_t path, uintptr_t argv, uintptr_t envp)
{
    process_t *parent = scheduler_current()->process;

    address_space_t *space = NULL;
    uintptr_t entry, stack;
    long ret = program_load(path, argv, envp, &space, &entry, &stack);
    if (ret < 0)
        return ret;

    process_t *child = child_create(parent, space);
    if (child == NULL)
        return -ENOMEM;

    // spawn is a fork and an exec in one
    process_files_close_on_exec(child);
    signal_exec_reset(child);

    if (process_start(child, entry, stack) < 0)
    {
        process_child_remove(child);
        process_release(child);
        return -ENOMEM;
    }

    return child->pid;
}

/**
 * fork(): creates a new process by duplicating the calling process.
 *
 * @return The process ID of the newly created child process, or a negative error code on failure.
 */
long sys_fork(void)
{
    thread_t *thread = scheduler_current();
    process_t *parent = thread->process;

    address_space_t *space = address_space_clone(parent->space);
    if (space == NULL)
        return -ENOMEM;

    process_t *child = child_create(parent, space);
    if (child == NULL)
        return -ENOMEM;

    if (process_start_fork(child, thread) < 0)
    {
        process_child_remove(child);
        process_release(child);
        return -ENOMEM;
    }

    return child->pid;
}

/**
 * exec(path, argv, envp): replaces the current process image with a new executable.
 *
 * @param path The path to the executable.
 * @param argv The argument vector.
 * @param envp The environment vector.
 * @return 0 on success, or a negative error code on failure.
 */
long sys_exec(uintptr_t path, uintptr_t argv, uintptr_t envp)
{
    address_space_t *space = NULL;
    uintptr_t entry, stack;
    long ret = program_load(path, argv, envp, &space, &entry, &stack);
    if (ret < 0)
        return ret;

    process_exec(space, entry, stack);

    return 0;
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
