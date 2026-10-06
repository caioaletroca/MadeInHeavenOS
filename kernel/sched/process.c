#include <sched/process.h>
#include <sched/scheduler.h>
#include <sched/thread.h>
#include <asm/irq_flags.h>
#include <mm/kmalloc.h>
#include <mm/address_space.h>
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

process_t *process_create(address_space_t *space, uintptr_t entry, uintptr_t user_stack)
{
    process_t *p = kmalloc(sizeof(process_t));
    if (p == NULL)
        return NULL;

    p->pid = process_alloc_pid();
    p->state = PROCESS_RUNNING;
    p->exit_status = 0;
    p->refs = 2;
    p->space = space;
    wait_queue_init(&p->waiters);

    p->thread = thread_create_user(p, entry, user_stack);
    if (p->thread == NULL)
    {
        kfree(p);
        return NULL;
    }

    return p;
}

__attribute__((noreturn)) void process_exit(int status)
{
    process_t *current = scheduler_current()->process;
    current->exit_status = status;
    current->state = PROCESS_EXITED;

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
    if (--p->refs == 0)
    {
        address_space_destroy(p->space);
        kfree(p);
    }
    irq_restore(flags);
}