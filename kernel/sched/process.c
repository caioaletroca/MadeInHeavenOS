#include <stdbool.h>
#include <arch/context.h>
#include <sched/process.h>
#include <sched/scheduler.h>
#include <sched/thread.h>
#include <sched/signal.h>
#include <mm/kmalloc.h>
#include <mm/address_space.h>
#include <mihos/signal.h>
#include <mihos/wait.h>
#include <syscall.h>
#include <kprintf.h>

#define WAIT_STATUS_STOPPED 0x7F // POSIX status: the low byte of a child that stopped

static spinlock_t pid_lock = SPINLOCK_INIT;
static uint32_t next_pid = PROCESS_INIT_PID + 1;

// Protects parent, children and sibling of every process. Lock order: this
// first, then a process's lock (never take this while holding p->lock).
static spinlock_t process_tree_lock = SPINLOCK_INIT;

// Every process, for kill to find by pid. Protected by process_tree_lock.
static list_t all_processes = {.next = &all_processes, .prev = &all_processes};

// /sbin/init once process_create_init made it; adopts orphans. Set before init
// can run, never cleared: init exiting panics the kernel (init_start).
static process_t *init_process;

/**
 * @brief Allocates a new process ID in a thread-safe manner.
 *
 * @return The newly allocated process ID.
 */
static uint32_t process_alloc_pid(void)
{
    guard(spinlock, &pid_lock);
    return next_pid++;
}

/**
 * @brief Closes all open file descriptors for the given process.
 *
 * @param p The process whose open file descriptors are to be closed.
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

/**
 * @brief Releases the current working directory of the given process.
 *
 * @param p The process whose current working directory is to be released.
 */
static void process_cwd_put(process_t *p)
{
    if (p->cwd != NULL)
    {
        vfs_node_put(p->cwd);
        p->cwd = NULL;
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
    p->group = p->pid;
    p->session = p->pid;
    list_init(&p->children);
    list_init(&p->sibling);
    wait_queue_init(&p->waiters);
    wait_queue_init(&p->child_exits);
    wait_queue_init(&p->resume_waiters);
    spinlock_init(&p->lock);

    list_init(&p->all_link);
    scoped_guard(spinlock, &process_tree_lock)
    {
        list_insert_before(&all_processes, &p->all_link);
    }

    return p;
}

process_t *process_create_init(address_space_t *space)
{
    process_t *p = process_create(space);
    if (p != NULL)
    {
        p->pid = PROCESS_INIT_PID;
        p->group = PROCESS_INIT_PID;
        p->session = PROCESS_INIT_PID;
        init_process = p;
    }

    return p;
}

/**
 * @brief Looks up a process by its PID without taking a reference.
 *
 * @param pid The PID of the process to look up.
 * @return The process with the given PID, or NULL if not found.
 */
static process_t *process_lookup(uint32_t pid)
{
    list_t *pos;
    list_for_each(pos, &all_processes)
    {
        process_t *p = list_container(pos, process_t, all_link);
        if (p->pid == pid)
            return p;
    }

    return NULL;
}

process_t *process_find(uint32_t pid)
{
    scoped_guard(spinlock, &process_tree_lock)
    {
        process_t *p = process_lookup(pid);
        if (p != NULL)
        {
            scoped_guard(spinlock, &p->lock)
            {
                p->refs++;
            }
        }

        return p;
    }

    return NULL;
}

/**
 * @brief Checks if a process group exists within a session.
 *
 * @param group The process group ID to check.
 * @param session The session ID to check within.
 * @return true if the process group exists, false otherwise.
 */
static bool process_group_exists(uint32_t group, uint32_t session)
{
    list_t *pos;
    list_for_each(pos, &all_processes)
    {
        process_t *p = list_container(pos, process_t, all_link);
        if (p->group == group && p->session == session)
            return true;
    }

    return false;
}

void process_group_inherit(process_t *child, process_t *parent)
{
    guard(spinlock, &process_tree_lock);
    child->group = parent->group;
    child->session = parent->session;
}

long process_group_set(process_t *caller, uint32_t pid, uint32_t group)
{
    guard(spinlock, &process_tree_lock);

    process_t *target = pid == 0 ? caller : process_lookup(pid);

    // Only yourself or your own child
    if (target == NULL || (target != caller && target->parent != caller))
        return -ESRCH;

    if (target->session != caller->session)
        return -EPERM;

    // A running program cannot have its group changed under it
    if (target != caller && target->exec_done)
        return -EACCES;

    // A leader stays in its own group: the session's name depends on it
    if (target->pid == target->session)
        return -EPERM;

    if (group == 0)
        group = target->pid;

    // Joining needs an existing group of this session; only its own name can be new
    if (group != target->pid && !process_group_exists(group, caller->session))
        return -EPERM;

    target->group = group;
    return 0;
}

long process_group_get(process_t *caller, uint32_t pid)
{
    guard(spinlock, &process_tree_lock);

    process_t *p = pid == 0 ? caller : process_lookup(pid);
    if (p == NULL)
        return -ESRCH;

    return p->group;
}

long process_session_create(process_t *p)
{
    guard(spinlock, &process_tree_lock);

    if (process_group_exists(p->pid, p->session))
        return -EPERM;

    p->session = p->pid;
    p->group = p->pid;
    return p->pid;
}

long process_session_get(process_t *caller, uint32_t pid)
{
    guard(spinlock, &process_tree_lock);

    process_t *p = pid == 0 ? caller : process_lookup(pid);
    if (p == NULL)
        return -ESRCH;

    return p->session;
}

bool process_group_in_session(uint32_t group, uint32_t session)
{
    guard(spinlock, &process_tree_lock);
    return process_group_exists(group, session);
}

bool process_session_leader_alive(uint32_t session)
{
    guard(spinlock, &process_tree_lock);

    process_t *leader = process_lookup(session);
    return leader != NULL && leader->state != PROCESS_EXITED;
}

long process_signal_group(uint32_t group, int signal)
{
    long result = -ESRCH;

    guard(spinlock, &process_tree_lock);

    list_t *pos;
    list_for_each(pos, &all_processes)
    {
        process_t *p = list_container(pos, process_t, all_link);
        if (p->group != group)
            continue;

        result = 0;
        if (signal != 0)
            signal_send(p, signal);
    }

    return result;
}

long process_signal_everyone(process_t *caller, int signal)
{
    long result = -ESRCH;

    guard(spinlock, &process_tree_lock);

    list_t *pos;
    list_for_each(pos, &all_processes)
    {
        process_t *p = list_container(pos, process_t, all_link);
        if (p == caller || p->pid == PROCESS_INIT_PID)
            continue;

        result = 0;
        if (signal != 0)
            signal_send(p, signal);
    }

    return result;
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
            p->close_on_exec &= ~(1u << i);
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

int process_start_fork(process_t *p, thread_t *parent)
{
    if (p == NULL)
        return -1;

    scoped_guard(spinlock, &p->lock)
    {
        p->refs++;
    }

    p->thread = thread_create_fork(p, parent);
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

    child->close_on_exec = parent->close_on_exec;
}

void process_files_close_on_exec(process_t *p)
{
    for (int fd = 0; fd < PROCESS_MAX_FILES; fd++)
    {
        if ((p->close_on_exec & (1u << fd)) != 0 && p->files[fd] != NULL)
        {
            file_put(p->files[fd]);
            p->files[fd] = NULL;
        }
    }

    p->close_on_exec = 0;
}

void process_exec(address_space_t *space, uintptr_t entry, uintptr_t user_stack)
{
    thread_t *thread = scheduler_current();
    process_t *p = thread->process;

    address_space_t *old = p->space;
    p->space = space;
    thread->space = space;

    // The old space must not be active when it is destroyed
    address_space_activate(space);
    address_space_destroy(old);

    process_files_close_on_exec(p);
    p->exec_done = true;
    signal_exec_reset(p);

    // A trap from ring 3 starts at the top of the kernel stack, so this
    // overwrites the frame of the running system call: its iretq enters the program
    arch_user_context_init(thread->stack, thread->stack_size, entry, user_stack);
}

void process_cwd_inherit(process_t *child, process_t *parent)
{
    child->cwd = parent->cwd;
    if (child->cwd != NULL)
        vfs_node_get(child->cwd);
}

void process_cwd_set(process_t *p, vfs_node_t *dir)
{
    vfs_node_t *old = p->cwd;
    p->cwd = dir;
    if (old != NULL)
        vfs_node_put(old);
}

void process_child_add(process_t *parent, process_t *child)
{
    scoped_guard(spinlock, &process_tree_lock)
    {
        list_insert_before(&parent->children, &child->sibling);
        child->parent = parent;
    }
}

void process_child_remove(process_t *child)
{
    scoped_guard(spinlock, &process_tree_lock)
    {
        list_remove(&child->sibling);
        child->parent = NULL;
    }
}

/**
 * Checks if a child process matches the given wait criteria.
 *
 * @param parent The parent process.
 * @param child The child process to check.
 * @param pid The PID or process group to match against.
 * @return true if the child matches the criteria, false otherwise.
 */
static bool process_wait_matches(process_t *parent, process_t *child, long pid)
{
    if (pid == -1)
        return true;
    if (pid > 0)
        return (long)child->pid == pid;

    uint32_t group = pid == 0 ? parent->group : (uint32_t)-pid;
    return child->group == group;
}

long process_wait_child(process_t *parent, long pid, int options, process_t **out, int *stop_status)
{
    scoped_guard(spinlock, &process_tree_lock)
    {
        int interrupted = 0;
        for (;;)
        {
            bool found = false;

            list_t *pos;
            list_for_each(pos, &parent->children)
            {
                process_t *p = list_container(pos, process_t, sibling);
                if (!process_wait_matches(parent, p, pid))
                    continue;

                found = true;

                if (p->state == PROCESS_EXITED)
                {
                    list_remove(&p->sibling);
                    list_remove(&p->all_link);
                    p->parent = NULL;
                    *out = p;
                    return p->pid;
                }

                if ((options & WUNTRACED) != 0 && p->stopped && !p->stop_reported)
                {
                    p->stop_reported = true;
                    *stop_status = (p->stop_signal << 8) | WAIT_STATUS_STOPPED;
                    return p->pid;
                }
            }

            if (!found)
                return -ECHILD;

            if ((options & WNOHANG) != 0)
                return 0;

            // A second pass, after the scan: the child that woke us beats the signal that came with it
            if (interrupted < 0)
                return interrupted;

            // Releases the lock while asleep, retakes it before returning; then scan again
            interrupted = wait_queue_sleep_interruptible(&parent->child_exits, &process_tree_lock);
        }
    }

    return -ECHILD;
}

uint32_t process_parent_pid(process_t *p)
{
    scoped_guard(spinlock, &process_tree_lock)
    {
        return p->parent ? p->parent->pid : 0;
    }

    return 0; // Unreachable, but required to satisfy the compiler
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

    list_t *pos, *next, orphans;
    list_init(&orphans);
    scoped_guard(spinlock, &process_tree_lock)
    {
        // The parent may be asleep in wait for this child (or any child)
        if (current->parent != NULL)
        {
            wait_queue_wake_all(&current->parent->child_exits);
            signal_send(current->parent, SIGCHLD);
        }

        // Remaining children go to init with their reference; init reaps them with wait(-1).
        // No adopter before init exists (kernel self-tests) or when init itself exits (the
        // kernel panics next): those children are dropped instead.
        process_t *adopter = (init_process != NULL && current != init_process) ? init_process : NULL;
        bool wake_adopter = false;

        list_for_each_safe(pos, next, &current->children)
        {
            process_t *child = list_container(pos, process_t, sibling);
            list_remove(&child->sibling);
            if (adopter != NULL)
            {
                child->parent = adopter;
                list_insert_before(&adopter->children, &child->sibling);
                // A zombie already exited: no later exit will wake init for it
                if (child->state == PROCESS_EXITED)
                    wake_adopter = true;
            }
            else
            {
                child->parent = NULL;
                list_insert_before(&orphans, &child->sibling);
            }
        }

        if (wake_adopter)
            wait_queue_wake_all(&adopter->child_exits);
    }

    // One thread per process: only this thread touches the table now (needs a lock with threads)
    process_close_files(current);

    // Release the current working directory of the process
    process_cwd_put(current);

    // Dropped children: release our reference after the lock; each is freed once it exits too
    list_for_each_safe(pos, next, &orphans)
    {
        process_t *child = list_container(pos, process_t, sibling);
        process_release(child);
    }

    thread_exit();
}

__attribute__((noreturn)) void process_exit_signal(int signal)
{
    process_t *current = scheduler_current()->process;
    current->signal = signal;
    process_exit(SIGNAL_EXIT_STATUS(signal));
}

void process_stop(int signal)
{
    process_t *current = scheduler_current()->process;

    scoped_guard(spinlock, &process_tree_lock)
    {
        scoped_guard(spinlock, &current->lock)
        {
            current->stopped = true;
            current->stop_reported = false;
            current->stop_signal = signal;
        }

        if (current->parent != NULL)
        {
            wait_queue_wake_all(&current->parent->child_exits);
            signal_send(current->parent, SIGCHLD);
        }
    }

    guard(spinlock, &current->lock);
    while (current->stopped)
        wait_queue_sleep_locked(&current->resume_waiters, &current->lock);
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

    // Tree lock first: kill must never find a process that is being freed
    scoped_guard(spinlock, &process_tree_lock)
    {
        scoped_guard(spinlock, &p->lock)
        {
            last = (--p->refs == 0);
        }
        if (last)
            list_remove(&p->all_link);
    }

    if (last)
    {
        process_cwd_put(p);
        process_close_files(p);
        address_space_destroy(p->space);
        kfree(p);
    }
}