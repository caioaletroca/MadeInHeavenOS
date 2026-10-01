#ifndef _THREAD_H_
#define _THREAD_H_

#include <stddef.h>
#include <stdint.h>
#include <sys/list.h>

typedef void (*thread_entry_t)(void);

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
 * @brief Thread structure representing a single thread in the system.
 */
typedef struct thread
{
    uint32_t id;
    void *stack;
    size_t stack_size;
    void *context; // Saved execution context (opaque, arch-defined)
    thread_entry_t entry;
    thread_state_t state;
    list_t run_link;
} thread_t;

/**
 * @brief Initialize a thread structure with the given stack, stack size, and entry point.
 *
 * @param thread Pointer to the thread structure to initialize.
 * @param stack Pointer to the stack memory for the thread.
 * @param stack_size Size of the stack memory for the thread.
 * @param entry Entry point function for the thread.
 *
 * @return 0 on success, or a negative error code on failure.
 */
int thread_init(thread_t *thread, void *stack, size_t stack_size, thread_entry_t entry);

/**
 * @brief Start the execution of the current thread. This function should never return.
 */
void thread_start(void);

__attribute__((noreturn)) void thread_exit(void);

#endif // _THREAD_H_