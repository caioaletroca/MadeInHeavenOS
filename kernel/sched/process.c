#include <sched/process.h>
#include <sched/scheduler.h>
#include <sched/thread.h>
#include <asm/irq_flags.h>
#include <mm/kmalloc.h>
#include <mm/address_space.h>
#include <syscall.h>
#include <kprintf.h>

static uint32_t next_pid = 1;

/**
 * Allocates a new process ID in a thread-safe manner.
 */
static uint32_t process_alloc_pid(void)
{
    irq_flags_t flags = irq_save();
    uint32_t pid = next_pid++;
    irq_restore(flags);
    return pid;
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

    irq_flags_t flags = irq_save();
    p->refs++;
    irq_restore(flags);

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
    current->exit_status = status;
    current->state = PROCESS_EXITED;

    // One thread per process: only this thread touches the table now (needs a lock with threads or fork)
    process_close_files(current);

    kprintf("Process %u exited with status %d\n", current->pid, status);

    wait_queue_wake_all(&current->waiters);
    thread_exit();
}

int process_wait(process_t *p)
{
    if (p == NULL)
        return -1;

    irq_flags_t flags = irq_save();

    while (p->state != PROCESS_EXITED)
        wait_queue_sleep(&p->waiters);

    irq_restore(flags);

    return p->exit_status;
}

void process_release(process_t *p)
{
    if (p == NULL)
        return;

    irq_flags_t flags = irq_save();
    bool last = (--p->refs == 0);
    irq_restore(flags);

    if (last)
    {
        process_close_files(p);
        address_space_destroy(p->space);
        kfree(p);
    }
}