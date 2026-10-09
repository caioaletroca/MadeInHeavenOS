#include <mihos/signal.h>
#include <sched/process.h>
#include <sched/scheduler.h>
#include <sched/thread.h>
#include <mm/kmalloc.h>
#include <mm/address_space.h>
#include <syscall.h>
#include <kprintf.h>

static spinlock_t pid_lock = SPINLOCK_INIT;
static uint32_t next_pid = PROCESS_INIT_PID + 1;

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
    p->signal = 0;
    p->refs = 1;
    p->space = space;
    p->parent = NULL;
    list_init(&p->children);
    list_init(&p->sibling);
    wait_queue_init(&p->waiters);
    spinlock_init(&p->lock);

    return p;
}

process_t *process_create_init(address_space_t *space)
{
    process_t *p = process_create(space);
    if (p != NULL)
        p->pid = PROCESS_INIT_PID;

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

void process_files_inherit(process_t *child, process_t *parent)
{
    for (int fd = 0; fd < PROCESS_MAX_FILES; fd++)
        if (parent->files[fd] != NULL)
            child->files[fd] = file_get(parent->files[fd]);
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

    // Children nobody will wait for: drop our reference; each is freed once it exits too
    list_t *pos, *next;
    list_for_each_safe(pos, next, &current->children)
    {
        process_t *child = list_container(pos, process_t, sibling);
        list_delete(&child->sibling);
        child->parent = NULL;
        process_release(child);
    }

    kprintf("Process %u exited with status %d\n", current->pid, status);

    thread_exit();
}

__attribute__((noreturn)) void process_exit_signal(int signal)
{
    process_t *current = scheduler_current()->process;
    current->signal = signal;
    process_exit(SIGNAL_EXIT_STATUS(signal));
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