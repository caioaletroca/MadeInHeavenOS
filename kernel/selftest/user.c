#include "selftest/user.h"
#include <addresses.h>
#include <asm/memory.h>
#include <arch/mmu.h>
#include <driver/timer.h>
#include <mm/address_space.h>
#include <sched/thread.h>
#include <syscall.h>
#include <kprintf.h>
#include <panic.h>

// Program images from user_programs.S (copied to USER_BASE, never run in place)
extern const uint8_t user_loop_start[], user_loop_counter[], user_loop_stop[], user_loop_end[];
extern const uint8_t user_hello_start[], user_hello_result[], user_hello_end[];

/**
 * Load a program image at USER_BASE with a one-page stack, and start it.
 * The space is not freed: without a join, the test cannot know when the
 * thread is gone (processes will own their space from 4f on).
 */
static address_space_t *user_program_start(const uint8_t *start, const uint8_t *end)
{
    size_t size = end - start;
    address_space_t *space = address_space_create();

    if (space == NULL ||
        address_space_map(space, USER_BASE, size, MMU_WRITE | MMU_EXEC) != 0 ||
        address_space_map(space, USER_STACK_TOP - PAGE_SIZE, PAGE_SIZE, MMU_WRITE) != 0 ||
        address_space_write(space, USER_BASE, start, size) != 0 ||
        thread_create_user(space, USER_BASE, USER_STACK_TOP) == NULL)
        panic("Failed to start user program\n");

    return space;
}

/**
 * Read a 64-bit variable of a running program, given its label in the image.
 */
static uint64_t user_variable_read(address_space_t *space, const uint8_t *start, const uint8_t *label)
{
    uint64_t value;

    if (address_space_read(space, USER_BASE + (label - start), &value, sizeof(value)) != 0)
        panic("User variable is not mapped\n");

    return value;
}

static void user_loop_test(void)
{
    address_space_t *space = user_program_start(user_loop_start, user_loop_end);
    uint64_t previous = 0;

    for (int i = 0; i < 3; i++)
    {
        thread_sleep(TIMER_FREQUENCY_HZ);

        uint64_t counter = user_variable_read(space, user_loop_start, user_loop_counter);
        kprintf("User mode counter: %u\n", (unsigned int)counter);

        if (counter <= previous)
            panic("User thread is not making progress\n");

        previous = counter;
    }

    // Ask the loop to exit(0)
    static const uint8_t stop = 1;
    address_space_write(space, USER_BASE + (user_loop_stop - user_loop_start), &stop, sizeof(stop));
    thread_sleep(TIMER_FREQUENCY_HZ / 10);
}

static void user_syscall_test(void)
{
    address_space_t *space = user_program_start(user_hello_start, user_hello_end);

    thread_sleep(TIMER_FREQUENCY_HZ / 10);

    int64_t result = (int64_t)user_variable_read(space, user_hello_start, user_hello_result);

    if (result != -EFAULT)
        panic("Kernel pointer write returned %d instead of -EFAULT\n", (int)result);
}

void user_selftest(void)
{
    user_loop_test();
    user_syscall_test();

    kprintf("User mode self-test completed successfully\n");
}