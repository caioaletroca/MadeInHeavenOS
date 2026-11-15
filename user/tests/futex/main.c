#include <sys/mman.h>
#include <sys/wait.h>
#include <mihos/syscall.h>
#include <errno.h>
#include <signal.h>
#include <stdint.h>
#include <time.h>
#include <unistd.h>
#include <test.h>

/*
 * The futex: futex_wait sleeps while a 32-bit word holds the expected value,
 * futex_wake wakes sleepers on that word. mlibc's locks are built on them.
 * With one thread per process nobody else can wake us, so this covers the
 * ways a wait ends without a wake (the word differs, the time limit, a signal)
 * and the errors; the kernel self-test covers a real wake between two threads.
 * Our libc has no wrapper: the calls are made raw, as mlibc's sysdeps will.
 */

#define SHORT_LIMIT_MS 100
#define FORK_LIMIT_MS 300
#define LONG_LIMIT_MS 2000
#define SIGNAL_DELAY_MS 50
#define SLACK_MS 30

static long futex_wait(volatile uint32_t *word, uint32_t expected, const struct timespec *limit)
{
    long result;
    __asm__ __volatile__("int $0x80"
                         : "=a"(result)
                         : "a"((long)SYS_FUTEX_WAIT), "D"(word), "S"((long)expected), "d"(limit)
                         : "rcx", "r11", "memory");
    return result;
}

static long futex_wake(volatile uint32_t *word, long count)
{
    long result;
    __asm__ __volatile__("int $0x80"
                         : "=a"(result)
                         : "a"((long)SYS_FUTEX_WAKE), "D"(word), "S"(count)
                         : "rcx", "r11", "memory");
    return result;
}

static long now_ms(void)
{
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec * 1000L + t.tv_nsec / 1000000L;
}

static struct timespec ms_limit(long ms)
{
    return (struct timespec){ms / 1000, (ms % 1000) * 1000000L};
}

static void sleep_ms(long ms)
{
    struct timespec t = ms_limit(ms);
    nanosleep(&t, NULL);
}

static volatile uint32_t word;

// The word differs already: no sleep. A zero time limit only checks it
static void test_no_sleep(void)
{
    word = 5;
    CHECK(futex_wait(&word, 4, NULL) == -EAGAIN);

    struct timespec zero = {0, 0};
    long start = now_ms();
    CHECK(futex_wait(&word, 5, &zero) == -ETIMEDOUT);
    CHECK(now_ms() - start < SLACK_MS);
    CHECK(futex_wait(&word, 4, &zero) == -EAGAIN); // the check comes first
}

// Nobody wakes it: the time limit ends the wait, not before
static void test_time_limit(void)
{
    word = 0;
    struct timespec limit = ms_limit(SHORT_LIMIT_MS);
    long start = now_ms();
    CHECK(futex_wait(&word, 0, &limit) == -ETIMEDOUT);
    long elapsed = now_ms() - start;
    CHECK(elapsed >= SHORT_LIMIT_MS && elapsed < SHORT_LIMIT_MS + 10 * SLACK_MS);
}

static void test_wake_nobody(void)
{
    CHECK(futex_wake(&word, 1) == 0);
    CHECK(futex_wake(&word, 1000) == 0);
}

// A page touched for the first time: the kernel maps it while checking the word
static void test_fresh_page(void)
{
    volatile uint32_t *page = mmap(NULL, 4096, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    CHECK(page != MAP_FAILED);
    if (page == MAP_FAILED)
        return;

    struct timespec zero = {0, 0};
    CHECK(futex_wait(page, 0, &zero) == -ETIMEDOUT); // a new page reads 0
    CHECK(futex_wait(page, 1, NULL) == -EAGAIN);
    munmap((void *)page, 4096);
}

static volatile int handled;

static void on_signal(int signal)
{
    (void)signal;
    handled = 1;
}

/*
 * A signal ends the wait with EINTR, even with SA_RESTART: lock code checks
 * the word again and decides (a restart would sleep again on a word that may
 * have changed meanwhile). Wrongly restarted, the wait would run to its limit.
 */
static void test_signal(void)
{
    struct sigaction action = {0};
    action.sa_handler = on_signal;
    action.sa_flags = SA_RESTART;
    sigemptyset(&action.sa_mask);
    CHECK(sigaction(SIGUSR1, &action, NULL) == 0);

    pid_t parent = getpid();
    fflush(stdout);
    pid_t pid = fork();
    if (pid == 0)
    {
        sleep_ms(SIGNAL_DELAY_MS);
        kill(parent, SIGUSR1);
        _exit(0);
    }
    CHECK(pid > 0);

    word = 0;
    handled = 0;
    struct timespec limit = ms_limit(LONG_LIMIT_MS);
    long start = now_ms();
    CHECK(futex_wait(&word, 0, &limit) == -EINTR);
    CHECK(now_ms() - start < LONG_LIMIT_MS / 2);
    CHECK(handled == 1);

    int status = -1;
    CHECK(waitpid(pid, &status, 0) == pid && WIFEXITED(status));
    signal(SIGUSR1, SIG_DFL);
}

/*
 * The key is the address in its address space: a fork child's word is a copy
 * at the same address, and its wake finds nobody, so the parent sleeps on.
 */
static void test_fork_is_apart(void)
{
    word = 0;
    fflush(stdout);
    pid_t pid = fork();
    if (pid == 0)
    {
        sleep_ms(SIGNAL_DELAY_MS);
        word = 1;
        _exit(futex_wake(&word, 1) == 0 ? 0 : 1);
    }
    CHECK(pid > 0);

    struct timespec limit = ms_limit(FORK_LIMIT_MS);
    CHECK(futex_wait(&word, 0, &limit) == -ETIMEDOUT);
    CHECK(word == 0);

    int status = -1;
    CHECK(waitpid(pid, &status, 0) == pid);
    CHECK(WIFEXITED(status) && WEXITSTATUS(status) == 0);
}

static void test_errors(void)
{
    volatile uint32_t pair[2] = {0, 0};
    volatile uint32_t *unaligned = (volatile uint32_t *)((char *)pair + 2);
    CHECK(futex_wait(unaligned, 0, NULL) == -EINVAL);
    CHECK(futex_wake(unaligned, 1) == -EINVAL);

    struct timespec bad = {0, 1000000000L};
    CHECK(futex_wait(&word, word, &bad) == -EINVAL);
    bad = (struct timespec){-1, 0};
    CHECK(futex_wait(&word, word, &bad) == -EINVAL);

    CHECK(futex_wait((volatile uint32_t *)0, 0, NULL) == -EFAULT);
    CHECK(futex_wait(&word, word, (const struct timespec *)8) == -EFAULT);
}

int main(void)
{
    test_no_sleep();
    test_time_limit();
    test_wake_nobody();
    test_fresh_page();
    test_signal();
    test_fork_is_apart();
    test_errors();
    return test_status();
}
