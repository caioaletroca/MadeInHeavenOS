#include "selftest/user.h"
#include <addresses.h>
#include <asm/memory.h>
#include <arch/mmu.h>
#include <driver/timer.h>
#include <driver/console.h>
#include <mm/address_space.h>
#include <sched/process.h>
#include <sched/thread.h>
#include <kprintf.h>
#include <panic.h>

// The program image from user_program.S (copied to USER_BASE, never run in place). Faults,
// EFAULT and the fd table are tested by the libc programs in user/tests/ (spawn, files)
extern const uint8_t user_loop_start[], user_loop_counter[], user_loop_stop[], user_loop_end[];

/**
 * Load a program image at USER_BASE with a one-page stack, and start it as a
 * process. The caller holds a reference: wait for it, then release it.
 */
static process_t *user_program_start(const uint8_t *start, const uint8_t *end)
{
    size_t size = end - start;
    address_space_t *space = address_space_create();

    if (space == NULL ||
        address_space_map(space, USER_BASE, size, MMU_WRITE | MMU_EXEC) != 0 ||
        address_space_map(space, USER_STACK_TOP - PAGE_SIZE, PAGE_SIZE, MMU_WRITE) != 0 ||
        address_space_write(space, USER_BASE, start, size) != 0)
        panic("Failed to load user program\n");

    process_t *process = process_create(space);
    if (process == NULL)
        panic("Failed to start user program\n");

    for (int i = 0; i < 3; i++)
        if (process_fd_install(process, console_file()) != i)
            panic("Failed to install fd %d\n", i);

    if (process_start(process, USER_BASE, USER_STACK_TOP) != 0)
        panic("Failed to start user program\n");

    return process;
}

/**
 * Read a 64-bit variable of a program, given its label in the image.
 * Valid until the process is released, even after it exited.
 */
static uint64_t user_variable_read(process_t *process, const uint8_t *start, const uint8_t *label)
{
    uint64_t value;

    if (address_space_read(process->space, USER_BASE + (label - start), &value, sizeof(value)) != 0)
        panic("User variable is not mapped\n");

    return value;
}

static void user_loop_test(void)
{
    process_t *process = user_program_start(user_loop_start, user_loop_end);
    uint64_t previous = 0;

    for (int i = 0; i < 3; i++)
    {
        thread_sleep(TIMER_FREQUENCY_HZ);

        uint64_t counter = user_variable_read(process, user_loop_start, user_loop_counter);
        if (counter <= previous)
            panic("User thread is not making progress\n");

        previous = counter;
    }

    // Ask the loop to exit(0)
    static const uint8_t stop = 1;
    address_space_write(process->space, USER_BASE + (user_loop_stop - user_loop_start), &stop, sizeof(stop));

    int status = process_wait(process);

    if (status != 0)
        panic("Loop program exited with status %d instead of 0\n", status);

    process_release(process);
}

void user_selftest(void)
{
    user_loop_test();
}
