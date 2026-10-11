#include <syscall.h>
#include <addresses.h>
#include <asm/memory.h>
#include <mm/address_space.h>
#include <mm/kmalloc.h>
#include <sched/scheduler.h>
#include <sched/thread.h>
#include <sched/process.h>
#include <exec/exec.h>
#include <fs/initrd.h>
#include <power.h>
#include <util.h>

// Bytes moved per copy; bounds the kernel stack buffer
#define SYSCALL_CHUNK_SIZE 256

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

/**
 * Returns the address space of the currently executing thread.
 */
static address_space_t *syscall_space(void)
{
    return scheduler_current()->space;
}

/**
 * user_string_copy(address, buffer, size): copies a null-terminated string from user space to the kernel buffer.
 * Returns the number of bytes copied on success, or a negative error code on failure.
 */
static long user_string_copy(uintptr_t address, char *buffer, size_t size)
{
    for (size_t i = 0; i < size; i++)
    {
        if (address_space_read(syscall_space(), address + i, &buffer[i], 1) != 0)
            return -EFAULT;

        if (buffer[i] == '\0')
            return (long)i;
    }
    return -E2BIG;
}

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
 * sys_file(fd): returns the file structure associated with the given file descriptor for the current process.
 */
static file_t *sys_file(unsigned long fd)
{
    process_t *p = scheduler_current()->process;
    return fd < PROCESS_MAX_FILES ? p->files[fd] : NULL;
}

/**
 * brk(address): changes the end of the data segment (heap) to the specified address.
 */
static long sys_brk(uintptr_t address)
{
    address_space_t *space = syscall_space();
    if (space == NULL)
        return -ENOMEM;

    uintptr_t limit = USER_STACK_TOP - USER_STACK_SIZE - PAGE_SIZE;

    if (address < space->heap_start || address > limit)
        return (long)space->brk;

    uintptr_t old_end = ALIGN_UP(space->brk, PAGE_SIZE);
    uintptr_t new_end = ALIGN_UP(address, PAGE_SIZE);

    if (new_end > old_end && address_space_map(space, old_end, new_end - old_end, MMU_WRITE) != 0)
        return (long)space->brk;

    if (new_end < old_end)
        address_space_unmap(space, new_end, old_end - new_end);

    space->brk = address;
    return (long)space->brk;
}

/**
 * write(fd, buffer, size): writes data to the given file descriptor from the specified buffer.
 */
static long sys_write(unsigned long fd, uintptr_t buffer, size_t size)
{
    char chunk[SYSCALL_CHUNK_SIZE];
    size_t done = 0;

    file_t *file = sys_file(fd);
    if (file == NULL || !(file->flags & FILE_WRITE) || file->ops->write == NULL)
        return -EBADF;

    while (done < size)
    {
        size_t length = size - done;

        if (length > SYSCALL_CHUNK_SIZE)
            length = SYSCALL_CHUNK_SIZE;

        // A fault after some output reports what was written, like Linux
        if (address_space_read(syscall_space(), buffer + done, chunk, length) != 0)
            return done > 0 ? (long)done : -EFAULT;

        long status = file->ops->write(file, chunk, length);
        if (status < 0)
        {
            return done > 0 ? (long)done : status;
        }

        done += length;
    }

    return (long)done;
}

/**
 * read(fd, buffer, size): reads data from the given file descriptor into the specified buffer.
 */
static long sys_read(unsigned long fd, uintptr_t buffer, size_t size)
{
    char chunk[SYSCALL_CHUNK_SIZE];

    file_t *file = sys_file(fd);
    if (file == NULL || !(file->flags & FILE_READ) || file->ops->read == NULL)
        return -EBADF;

    if (size > SYSCALL_CHUNK_SIZE)
        size = SYSCALL_CHUNK_SIZE;

    long length = file->ops->read(file, chunk, size);
    if (length < 0)
        return length;

    if (address_space_write(syscall_space(), buffer, chunk, length) != 0)
        return -EFAULT;

    return length;
}

static long sys_close(unsigned long fd)
{
    process_t *p = scheduler_current()->process;

    file_t *file = sys_file(fd);
    if (file == NULL)
        return -EBADF;

    p->files[fd] = NULL;
    file_put(file);
    return 0;
}

/**
 * exit(status): terminates the current thread with the given status.
 */
static long sys_exit(long status)
{
    process_exit((int)status);
}

/**
 * spawn(path, argv, envp): creates a new process with the specified executable, arguments, and environment.
 */
static long sys_spawn(uintptr_t path, uintptr_t argv, uintptr_t envp)
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

    const void *image = NULL;
    size_t size = 0;
    if (ret >= 0)
    {
        image = initrd_find(args->path, &size);
        if (image == NULL)
            ret = -ENOENT;
    }

    address_space_t *space = NULL;
    uintptr_t entry, stack;
    if (ret >= 0)
        ret = exec_load(image, size, args->argv, args->envp, &space, &entry, &stack);

    kfree(args);
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
static long sys_wait(long pid, uintptr_t status)
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

static long sys_reboot(long command)
{
    process_t *current = scheduler_current()->process;

    if (current->pid != PROCESS_INIT_PID)
        return -EPERM;

    switch (command)
    {
    case REBOOT_HALT:
        system_halt();
        return 0;
    default:
        return -EINVAL;
    }
}

static long sys_getpid(void)
{
    process_t *current = scheduler_current()->process;
    return current->pid;
}

static long sys_getppid(void)
{
    process_t *current = scheduler_current()->process;
    return process_parent_pid(current);
}

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
    default:
        return -ENOSYS;
    }
}