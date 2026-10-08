#include <syscall.h>
#include <asm/memory.h>
#include <mm/address_space.h>
#include <sched/scheduler.h>
#include <sched/thread.h>
#include <sched/process.h>
#include <util.h>

// Bytes moved per copy; bounds the kernel stack buffer
#define SYSCALL_CHUNK_SIZE 256

/**
 * Returns the address space of the currently executing thread.
 */
static address_space_t *syscall_space(void)
{
    return scheduler_current()->space;
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
    default:
        return -ENOSYS;
    }
}