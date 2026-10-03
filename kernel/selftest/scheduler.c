#include <selftest/scheduler.h>
#include <driver/timer.h>
#include <sched/thread.h>
#include <sched/sync.h>
#include <asm/cpu.h>
#include <kprintf.h>
#include <assert.h>
#include <panic.h>

#define PREEMPT_TEST_ITERATIONS 1000000
#define PREEMPT_TEST_TIMEOUT_TICKS 500

#define MUTEX_TEST_ITERATIONS 1000
#define SLEEP_TEST_TICKS 10

static thread_t worker_a_thread, worker_b_thread;
static uint8_t worker_a_stack[THREAD_STACK_SIZE] __attribute__((aligned(16)));
static uint8_t worker_b_stack[THREAD_STACK_SIZE] __attribute__((aligned(16)));

static volatile uint64_t worker_a_counter;
static volatile uint64_t worker_b_counter;
static volatile int worker_a_done;
static volatile int worker_b_done;

static semaphore_t tests_done;
static mutex_t counter_lock;
static uint64_t shared_counter;

static void worker_a_entry(void *arg)
{
    (void)arg;

    for (uint64_t i = 0; i < PREEMPT_TEST_ITERATIONS; i++)
        worker_a_counter++;
    worker_a_done = 1;
}

static void worker_b_entry(void *arg)
{
    (void)arg;

    for (uint64_t i = 0; i < PREEMPT_TEST_ITERATIONS; i++)
        worker_b_counter++;
    worker_b_done = 1;
}

static void mutex_worker(void *arg)
{
    (void)arg;

    for (int i = 0; i < MUTEX_TEST_ITERATIONS; i++)
    {
        mutex_lock(&counter_lock);

        // Yield inside the critical section: the other worker must block on
        // the mutex instead of interleaving the read-modify-write
        uint64_t value = shared_counter;
        scheduler_yield();
        shared_counter = value + 1;

        mutex_unlock(&counter_lock);
    }

    semaphore_up(&tests_done);
}

static void sleep_worker(void *arg)
{
    (void)arg;

    uint64_t start = timer_ticks();
    thread_sleep(SLEEP_TEST_TICKS);
    KASSERT(timer_ticks() - start >= SLEEP_TEST_TICKS);

    semaphore_up(&tests_done);
}

static void sync_selftest(void)
{
    semaphore_init(&tests_done, 0);
    mutex_init(&counter_lock);
    shared_counter = 0;

    KASSERT(thread_create(mutex_worker, NULL) != NULL);
    KASSERT(thread_create(mutex_worker, NULL) != NULL);
    KASSERT(thread_create(sleep_worker, NULL) != NULL);

    // Boot thread blocks here; idle runs whenever everyone is waiting
    for (int i = 0; i < 3; i++)
        semaphore_down(&tests_done);

    KASSERT(shared_counter == 2 * MUTEX_TEST_ITERATIONS);

    kprintf("Sync self-test completed successfully\n");
}

void scheduler_selftest(void)
{
    if (thread_init(&worker_a_thread, worker_a_stack, sizeof(worker_a_stack), worker_a_entry, NULL) != 0)
        panic("Failed to initialize worker A thread");

    if (thread_init(&worker_b_thread, worker_b_stack, sizeof(worker_b_stack), worker_b_entry, NULL) != 0)
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

    sync_selftest();
}
