#include <selftest/scheduler.h>
#include <sched/thread.h>
#include <kprintf.h>

#define THREAD_STACK_SIZE (16 * 1024)

static thread_t boot_thread, second_thread;
static uint8_t second_thread_stack[THREAD_STACK_SIZE] __attribute__((aligned(16)));

static void second_thread_entry(void)
{
    kprintf("Second thread exiting\n");
}

__attribute__((noreturn)) void scheduler_selftest(void)
{
    kprintf("Scheduler self-test running\n");

    scheduler_init(&boot_thread);

    if (thread_init(&second_thread, second_thread_stack, sizeof(second_thread_stack), second_thread_entry) != 0)
        panic("Failed to initialize second thread");

    if (scheduler_add(&second_thread) != 0)
        panic("Failed to add second thread to the scheduler");

    kprintf("Scheduler self-test completed successfully\n");

    for (;;)
    {
        scheduler_yield();
    }
}