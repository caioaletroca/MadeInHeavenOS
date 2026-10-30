#ifndef _SCHED_PROCESS_H
#define _SCHED_PROCESS_H

#include <stdint.h>
#include <sched/wait.h>
#include <sched/spinlock.h>
#include <fs/file.h>
#include <fs/vfs.h>
#include <sys/list.h>

#define PROCESS_MAX_FILES 16

#define PROCESS_INIT_PID 1 // reserved for /sbin/init (Unix programs assume it)

typedef enum
{
    PROCESS_RUNNING,
    PROCESS_EXITED
} process_state_t;

/**
 * Represents a process in the system.
 */
typedef struct process
{
    uint32_t pid;
    process_state_t state;
    int exit_status;
    int signal; // signal that killed it, 0 if it exited (set before EXITED)
    unsigned int refs;
    struct address_space *space;
    struct thread *thread;
    struct process *parent; // NULL for processes the kernel started; init for orphans. Protected by process_tree_lock.
    list_t children;        // spawned or adopted, not yet waited for; one reference each. Protected by process_tree_lock.
    list_t sibling;         // link in parent->children. Protected by process_tree_lock.

    wait_queue_t waiters;
    wait_queue_t child_exits; // the parent sleeps here in wait; every child's exit wakes it

    struct file *files[PROCESS_MAX_FILES];
    struct vfs_node *cwd; // the current directory, where relative paths start (one reference); NULL: the root

    spinlock_t lock;
} process_t;

/**
 * Create a new process.
 *
 * @param space The address space for the new process.
 * @return A pointer to the newly created process, or NULL on failure.
 */
process_t *process_create(struct address_space *space);

/**
 * Create the initial process, typically used for /sbin/init.
 *
 * @param space The address space for the new process.
 * @return A pointer to the newly created initial process, or NULL on failure.
 */
process_t *process_create_init(struct address_space *space);

/**
 * Install a file descriptor for the specified process.
 *
 * @param p The process to install the file descriptor for.
 * @param file The file to associate with the process.
 * @return The file descriptor index on success, or a negative error code on failure.
 */
int process_fd_install(process_t *p, struct file *file);

/**
 * Start the specified process by setting its entry point and user stack.
 *
 * @param p The process to start.
 * @param entry The entry point of the process.
 * @param user_stack The user stack pointer for the process.
 * @return 0 on success, or a negative error code on failure.
 */
int process_start(process_t *p, uintptr_t entry, uintptr_t user_stack);

/**
 * Inherit open file descriptors from the parent process to the child process.
 *
 * @param child The child process.
 * @param parent The parent process.
 */
void process_files_inherit(process_t *child, process_t *parent);

/**
 * Give the child the parent's current directory (a reference of its own), as spawn does.
 *
 * @param child The child process.
 * @param parent The parent process.
 */
void process_cwd_inherit(process_t *child, process_t *parent);

/**
 * Make dir the process's current directory. Takes over the caller's reference
 * to dir and drops the one to the old directory.
 *
 * @param p The process.
 * @param dir A directory, with one reference for the process.
 */
void process_cwd_set(process_t *p, struct vfs_node *dir);

/**
 * Add a child process to its parent's list of children.
 *
 * @param parent The parent process.
 * @param child The child process to add.
 */
void process_child_add(process_t *parent, process_t *child);

/**
 * Remove a child process from its parent's list of children.
 *
 * @param child The child process to remove.
 */
void process_child_remove(process_t *child);

/**
 * Wait for a child of parent to exit and take it off the children list.
 *
 * @param parent The waiting process (the caller).
 * @param pid The child's pid, or -1 for any child.
 * @param out Set to the exited child; the caller now owns its reference and releases it.
 * @return The child's pid, or -ECHILD if no child matches (0 and < -1: no process groups yet).
 */
long process_wait_child(process_t *parent, long pid, process_t **out);

/**
 * Get the parent process ID of the specified process.
 *
 * @param p The process whose parent PID is to be retrieved.
 * @return The parent process ID, or 0 if the process has no parent.
 */
uint32_t process_parent_pid(process_t *p);

/**
 * Exit the current process with the given status.
 *
 * @param status The exit status of the process.
 */
__attribute__((noreturn)) void process_exit(int status);

/**
 * Exit the current process due to the specified signal.
 *
 * @param signal The signal causing the process to exit.
 */
__attribute__((noreturn)) void process_exit_signal(int signal);

/**
 * Wait for the specified process to exit and return its exit status.
 *
 * @param p The process to wait for.
 * @return The exit status of the process, or -1 if the process is NULL.
 */
int process_wait(process_t *p);

/**
 * Release a reference to the specified process.
 *
 * @param p The process to release.
 */
void process_release(process_t *p);

#endif // _SCHED_PROCESS_H