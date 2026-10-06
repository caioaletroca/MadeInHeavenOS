#include <sched/process.h>
#include <sched/scheduler.h>
#include <sched/thread.h>
#include <mm/kmalloc.h>
#include <mm/address_space.h>
#include <syscall.h>
#include <kprintf.h>

static spinlock_t pid_lock = SPINLOCK_INIT;
static uint32_t next_pid = 1;

/**
 * Allocates a new process ID in a thread-safe manner.
 */
static uint32_t process_alloc_pid(void)
{
    guard(spinlock, &pid_lock);
    return next_pid++;
}

/**
 * Closes all open file descriptors for the given process.
 */
static void process_close_files(process_t *p)
{
    if (p == NULL)
        return;

    for (int i = 0; i < PROCESS_MAX_FILES; i++)
    {
        if (p->files[i] != NULL)
        {
            file_put(p->files[i]);
            p->files[i] = NULL;
        }
    }
}

process_t *process_create(address_space_t *space)
{
    process_t *p = kzalloc(sizeof(process_t));
    if (p == NULL)
        return NULL;

    p->pid = process_alloc_pid();
    p->state = PROCESS_RUNNING;
    p->exit_status = 0;
    p->refs = 1;
    p->space = space;
    wait_queue_init(&p->waiters);
    spinlock_init(&p->lock);

    return p;
}

int process_fd_install(process_t *p, file_t *file)
{
    if (p == NULL || file == NULL)
        return -EMFILE;

    for (int i = 0; i < PROCESS_MAX_FILES; i++)
    {
        if (p->files[i] == NULL)
        {
            p->files[i] = file;
            return i;
        }
    }

    return -EMFILE;
}

int process_start(process_t *p, uintptr_t entry, uintptr_t user_stack)
{
    if (p == NULL)
        return -1;

    scoped_guard(spinlock, &p->lock)
    {
        p->refs++;
    }

    p->thread = thread_create_user(p, entry, user_stack);
    if (p->thread == NULL)
    {
        process_release(p);
        return -1;
    }

    return 0;
}

__attribute__((noreturn)) void process_exit(int status)
{
    process_t *current = scheduler_current()->process;

    scoped_guard(spinlock, &current->lock)
    {
        current->exit_status = status;
        current->state = PROCESS_EXITED;
        wait_queue_wake_all(&current->waiters);
    }

    // One thread per process: only this thread touches the table now (needs a lock with threads or fork)
    process_close_files(current);

    kprintf("Process %u exited with status %d\n", current->pid, status);

    thread_exit();
}

int process_wait(process_t *p)
{
    if (p == NULL)
        return -1;

    scoped_guard(spinlock, &p->lock)
    {
        while (p->state != PROCESS_EXITED)
            wait_queue_sleep_locked(&p->waiters, &p->lock);
    }

    return p->exit_status;
}

void process_release(process_t *p)
{
    if (p == NULL)
        return;

    bool last = false;

    scoped_guard(spinlock, &p->lock)
    {
        last = (--p->refs == 0);
    }

    if (last)
    {
        process_close_files(p);
        address_space_destroy(p->space);
        kfree(p);
    }
}