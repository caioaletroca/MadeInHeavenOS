#include "selftest/user.h"
#include <addresses.h>
#include <asm/memory.h>
#include <arch/mmu.h>
#include <driver/timer.h>
#include <mm/address_space.h>
#include <sched/process.h>
#include <sched/thread.h>
#include <signal.h>
#include <syscall.h>
#include <kprintf.h>
#include <panic.h>

// Program images from user_programs.S (copied to USER_BASE, never run in place)
extern const uint8_t user_loop_start[], user_loop_counter[], user_loop_stop[], user_loop_end[];
extern const uint8_t user_hello_start[], user_hello_result[], user_hello_end[];
extern const uint8_t user_null_start[], user_null_before[], user_null_after[], user_null_end[];
extern const uint8_t user_hlt_start[], user_hlt_before[], user_hlt_after[], user_hlt_end[];

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

    process_t *process = process_create(space, USER_BASE, USER_STACK_TOP);

    if (process == NULL)
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
        kprintf("User mode counter: %u\n", (unsigned int)counter);

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

static void user_syscall_test(void)
{
    process_t *process = user_program_start(user_hello_start, user_hello_end);
    int status = process_wait(process);
    int64_t result = (int64_t)user_variable_read(process, user_hello_start, user_hello_result);

    if (result != -EFAULT)
        panic("Kernel pointer write returned %d instead of -EFAULT\n", (int)result);
    if (status != -EFAULT)
        panic("Hello program exited with status %d instead of -EFAULT\n", status);

    process_release(process);
}

/**
 * Run a program that sets `before`, faults, then would set `after`.
 * It must die at the faulting instruction with the expected status.
 */
static void user_fault_test(const char *name, const uint8_t *start, const uint8_t *before_label,
                            const uint8_t *after_label, const uint8_t *end, int expected)
{
    process_t *process = user_program_start(start, end);
    int status = process_wait(process);
    uint64_t before = user_variable_read(process, start, before_label);
    uint64_t after = user_variable_read(process, start, after_label);

    if (before != 1)
        panic("%s program never ran\n", name);
    if (after != 0)
        panic("%s program survived its fault\n", name);
    if (status != expected)
        panic("%s program exited with status %d instead of %d\n", name, status, expected);

    process_release(process);
}

void user_selftest(void)
{
    user_loop_test();
    user_syscall_test();
    user_fault_test("Null", user_null_start, user_null_before, user_null_after, user_null_end,
                    SIGNAL_EXIT_STATUS(SIGSEGV));
    user_fault_test("Hlt", user_hlt_start, user_hlt_before, user_hlt_after, user_hlt_end,
                    SIGNAL_EXIT_STATUS(SIGSEGV));

    kprintf("User mode self-test completed successfully\n");
}
