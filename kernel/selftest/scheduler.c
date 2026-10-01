#include <selftest/scheduler.h>
#include <sched/thread.h>
#include <kprintf.h>
#include <driver/timer.h>

#define THREAD_STACK_SIZE (16 * 1024)
#define PREEMPT_TEST_ITERATIONS 1000000
#define PREEMPT_TEST_TIMEOUT_TICKS 500

static thread_t worker_a_thread, worker_b_thread;
static uint8_t worker_a_stack[THREAD_STACK_SIZE] __attribute__((aligned(16)));
static uint8_t worker_b_stack[THREAD_STACK_SIZE] __attribute__((aligned(16)));

static volatile uint64_t worker_a_counter;
static volatile uint64_t worker_b_counter;
static volatile int worker_a_done;
static volatile int worker_b_done;

static void worker_a_entry(void)
{
    for (uint64_t i = 0; i < PREEMPT_TEST_ITERATIONS; i++)
        worker_a_counter++;
    worker_a_done = 1;
}

static void worker_b_entry(void)
{
    for (uint64_t i = 0; i < PREEMPT_TEST_ITERATIONS; i++)
        worker_b_counter++;
    worker_b_done = 1;
}

__attribute__((noreturn)) void scheduler_selftest(void)
{
    kprintf("Scheduler preemption self-test running\n");

    if (thread_init(&worker_a_thread, worker_a_stack, sizeof(worker_a_stack), worker_a_entry) != 0)
        panic("Failed to initialize worker A thread");

    if (thread_init(&worker_b_thread, worker_b_stack, sizeof(worker_b_stack), worker_b_entry) != 0)
        panic("Failed to initialize worker B thread");

    if (scheduler_add(&worker_a_thread) != 0)
        panic("Failed to add worker A thread to the scheduler");

    if (scheduler_add(&worker_b_thread) != 0)
        panic("Failed to add worker B thread to the scheduler");

    uint64_t start_ticks = timer_ticks();

    while (!worker_a_done || !worker_b_done)
    {
        if (timer_ticks() - start_ticks > PREEMPT_TEST_TIMEOUT_TICKS)
            panic("Scheduler preemption self-test timed out");
    }

    if (worker_a_counter != PREEMPT_TEST_ITERATIONS)
        panic("Worker A counter mismatch");

    if (worker_b_counter != PREEMPT_TEST_ITERATIONS)
        panic("Worker B counter mismatch");

    if (scheduler_current()->state != THREAD_RUNNING)
        panic("Boot thread is not running after preemption test");

    kprintf("Scheduler preemption self-test completed successfully\n");

    for (;;)
    {
        __asm__ __volatile__("hlt");
    }
}
