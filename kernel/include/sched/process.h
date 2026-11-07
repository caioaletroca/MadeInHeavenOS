#ifndef _SCHED_PROCESS_H
#define _SCHED_PROCESS_H

#include <stdint.h>
#include <stdbool.h>
#include <sched/wait.h>
#include <sched/spinlock.h>
#include <mihos/signal.h>
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
    struct process *parent;                // NULL for processes the kernel started; init for orphans. Protected by process_tree_lock.
    list_t children;                       // spawned or adopted, not yet waited for; one reference each. Protected by process_tree_lock.
    list_t sibling;                        // link in parent->children. Protected by process_tree_lock.
    list_t all_link;                       // link in the list of all processes. Protected by process_tree_lock.
    uint32_t group;                        // process group: the pid of its first member. Protected by process_tree_lock.
    uint32_t session;                      // session: the pid of its leader. Protected by process_tree_lock.
    bool exec_done;                        // ran exec: its parent may no longer move it to another group
    bool stopped;                          // asleep on resume_waiters until SIGCONT or SIGKILL. Protected by lock.
    bool stop_reported;                    // wait already told the parent about this stop
    int stop_signal;                       // the signal that stopped it
    wait_queue_t resume_waiters;           // a stopped process sleeps here
    uint32_t signals_pending;              // one bit per signal sent and not yet acted on. Protected by lock.
    uint32_t signals_blocked;              // waits in signals_pending while set here. Protected by lock.
    struct sigaction signal_actions[NSIG]; // zeroed: every signal starts as SIG_DFL. Protected by lock.

    wait_queue_t waiters;
    wait_queue_t child_exits; // the parent sleeps here in wait; every child's exit or stop wakes it

    struct file *files[PROCESS_MAX_FILES];
    uint32_t close_on_exec; // bit n: close fd n at exec. Only meaningful for an open fd
    struct vfs_node *cwd;   // the current directory, where relative paths start (one reference); NULL: the root

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
 * Start a forked process: its thread resumes where parent's system call returns.
 *
 * @param p The child process.
 * @param parent The calling thread, inside its fork system call.
 * @return 0 on success, or a negative error code on failure.
 */
int process_start_fork(process_t *p, struct thread *parent);

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
 * Close every fd marked close-on-exec.
 *
 * @param p The process.
 */
void process_files_close_on_exec(process_t *p);

/**
 * Turn the calling process into a new program, which cannot fail: its address
 * space is replaced and its saved user registers rewritten, so the current
 * system call returns into the new program.
 *
 * @param space The new address space, taken over by the process.
 * @param entry User address where execution starts.
 * @param user_stack Initial user stack pointer.
 */
void process_exec(struct address_space *space, uintptr_t entry, uintptr_t user_stack);

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
 * Wait for a child of parent to exit or, with WUNTRACED, to stop.
 *
 * @param parent The waiting process (the caller).
 * @param pid The child's pid, -1 for any child, 0 for a child in the caller's group, < -1 for a child in group -pid.
 * @param options WNOHANG, WUNTRACED, or 0.
 * @param out Set to an exited child, taken off the children list: the caller now owns its
 *            reference and releases it. Left alone otherwise.
 * @param stop_status Set to the POSIX status of a child that stopped.
 * @return The child's pid, 0 with WNOHANG if no child changed, or -ECHILD if no child matches.
 */
long process_wait_child(process_t *parent, long pid, int options, process_t **out, int *stop_status);

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

/**
 * @brief Finds a process by pid, with a reference: process_release when done.
 *
 * @return The process, or NULL if no such process exists.
 */
process_t *process_find(uint32_t pid);

/**
 * Give the child the parent's group and session, as fork and spawn do.
 *
 * @param child The child process.
 * @param parent The parent process.
 */
void process_group_inherit(process_t *child, process_t *parent);

/**
 * setpgid: move a process to a group.
 *
 * @param caller The calling process.
 * @param pid The process to move, 0 for the caller.
 * @param group The group to join, 0 for the one named after the process.
 * @return 0 on success, or a negative error code on failure.
 */
long process_group_set(process_t *caller, uint32_t pid, uint32_t group);

/**
 * @param caller The calling process.
 * @param pid The process, 0 for the caller.
 * @return Its group, or -ESRCH.
 */
long process_group_get(process_t *caller, uint32_t pid);

/**
 * setsid: make p the leader of a new session and of a new group inside it.
 *
 * @param p The calling process.
 * @return The new session, or -EPERM if a group already carries p's pid.
 */
long process_session_create(process_t *p);

/**
 * @param caller The calling process.
 * @param pid The process, 0 for the caller.
 * @return Its session, or -ESRCH.
 */
long process_session_get(process_t *caller, uint32_t pid);

/**
 * Send a signal to every member of a group.
 *
 * @param group The group.
 * @param signal The signal, 0 only checks that the group exists.
 * @return 0 on success, or -ESRCH if the group has no members.
 */
long process_signal_group(uint32_t group, int signal);

/**
 * Send a signal to every process except init and the caller.
 *
 * @param caller The calling process.
 * @param signal The signal, 0 only checks that someone exists.
 * @return 0 on success, or -ESRCH if nobody was found.
 */
long process_signal_everyone(process_t *caller, int signal);

/**
 * @param group The group.
 * @param session The session.
 * @return true if the group has a member in the session.
 */
bool process_group_in_session(uint32_t group, uint32_t session);

/**
 * @param session The session.
 * @return true if its leader has not exited.
 */
bool process_session_leader_alive(uint32_t session);

/**
 * Stop the current process: tell its parent, then sleep until SIGCONT or SIGKILL.
 *
 * @param signal The signal that stopped it.
 */
void process_stop(int signal);

#endif // _SCHED_PROCESS_H