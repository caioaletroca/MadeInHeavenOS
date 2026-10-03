#ifndef _THREAD_H_
#define _THREAD_H_

#include <stddef.h>
#include <stdint.h>
#include <sys/list.h>

typedef void (*thread_entry_t)(void *arg);

// Default stack size for threads.
#define THREAD_STACK_SIZE (16 * 1024)

// Thread struct and stack come from kmalloc and are freed by the reaper
#define THREAD_FLAG_OWNED (1u << 0)

/**
 * @brief Thread states for the scheduler.
 */
typedef enum thread_state
{
    THREAD_READY,
    THREAD_RUNNING,
    THREAD_BLOCKED,
    THREAD_TERMINATED
} thread_state_t;

/**
 * @brief Forward declaration of the address space structure.
 */
struct address_space;

/**
 * @brief Thread structure representing a single thread in the system.
 */
typedef struct thread
{
    uint32_t id;
    uint32_t flags;
    void *stack;
    size_t stack_size;
    void *context; // Saved execution context (opaque, arch-defined)
    thread_entry_t entry;
    void *arg; // Argument to pass to the thread entry function
    thread_state_t state;
    uint64_t wake_tick;

    struct address_space *space;

    list_t run_link;
} thread_t;

/**
 * @brief Initialize the threading system, including the thread reaper.
 */
void threads_init(void);

/**
 * @brief Create a new thread with the given entry point and argument.
 *
 * @param entry Entry point function for the thread.
 * @param arg Argument to pass to the thread entry function.
 *
 * @return Pointer to the newly created thread, or NULL on failure.
 */
thread_t *thread_create(thread_entry_t entry, void *arg);

/**
 * @brief Create a thread that starts in user mode.
 *
 * @param space Address space the thread runs in (not owned by the thread).
 * @param entry User address where execution starts.
 * @param user_stack Initial user stack pointer.
 *
 * @return Pointer to the new thread, or NULL on failure.
 */
thread_t *thread_create_user(struct address_space *space, uintptr_t entry, uintptr_t user_stack);

/**
 * @brief Initialize a thread structure with the given stack, stack size, and entry point.
 *
 * @param thread Pointer to the thread structure to initialize.
 * @param stack Pointer to the stack memory for the thread.
 * @param stack_size Size of the stack memory for the thread.
 * @param entry Entry point function for the thread.
 * @param arg Argument to pass to the thread entry function.
 *
 * @return 0 on success, or a negative error code on failure.
 */
int thread_init(thread_t *thread, void *stack, size_t stack_size, thread_entry_t entry, void *arg);

/**
 * @brief Start the execution of the current thread. This function should never return.
 */
void thread_start(void);

__attribute__((noreturn)) void thread_exit(void);

/**
 * @brief Put the current thread to sleep for the specified number of ticks.
 *
 * @param ticks Number of ticks to sleep.
 */
void thread_sleep(uint64_t ticks);

#endif // _THREAD_H_