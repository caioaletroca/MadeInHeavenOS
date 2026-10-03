#include <syscall.h>
#include <driver/console.h>
#include <mm/address_space.h>
#include <sched/scheduler.h>
#include <sched/thread.h>
#include <kprintf.h>

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
 * write(fd, buffer, size): stdout and stderr both go to the console.
 */
static long sys_write(unsigned long fd, uintptr_t buffer, size_t size)
{
    char chunk[SYSCALL_CHUNK_SIZE];
    size_t done = 0;

    if (fd != 1 && fd != 2)
        return -EBADF;

    while (done < size)
    {
        size_t length = size - done;

        if (length > SYSCALL_CHUNK_SIZE)
            length = SYSCALL_CHUNK_SIZE;

        // A fault after some output reports what was written, like Linux
        if (address_space_read(syscall_space(), buffer + done, chunk, length) != 0)
            return done > 0 ? (long)done : -EFAULT;

        console_write(chunk, length);
        done += length;
    }

    return (long)done;
}

/**
 * read(fd, buffer, size): stdin comes from the console.
 */
static long sys_read(unsigned long fd, uintptr_t buffer, size_t size)
{
    char chunk[SYSCALL_CHUNK_SIZE];

    if (fd != 0)
        return -EBADF;

    if (size > SYSCALL_CHUNK_SIZE)
        size = SYSCALL_CHUNK_SIZE;

    size_t length = console_read(chunk, size);

    if (address_space_write(syscall_space(), buffer, chunk, length) != 0)
        return -EFAULT;

    return (long)length;
}

/**
 * exit(status): terminates the current thread with the given status.
 */
static long sys_exit(long status)
{
    kprintf("Thread %u exited with status %d\n", scheduler_current()->id, (int)status);
    thread_exit();
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
    case SYS_WRITE:
        return sys_write(arg0, arg1, arg2);
    case SYS_READ:
        return sys_read(arg0, arg1, arg2);
    default:
        return -ENOSYS;
    }
}