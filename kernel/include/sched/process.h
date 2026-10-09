#ifndef _SCHED_PROCESS_H
#define _SCHED_PROCESS_H

#include <stdint.h>
#include <sched/wait.h>
#include <sched/spinlock.h>
#include <fs/file.h>
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
    struct process *parent; // NULL for processes the kernel started
    list_t children;        // spawned, not yet waited for; one reference each.
    list_t sibling;         // link in parent->children

    wait_queue_t waiters;

    struct file *files[PROCESS_MAX_FILES];

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