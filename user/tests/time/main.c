#include <errno.h>
#include <signal.h>
#include <stdint.h>
#include <time.h>
#include <unistd.h>
#include <sys/wait.h>
#include <test.h>

/*
 * The clocks and sleeping: clock_gettime, time, nanosleep and sleep.
 *
 * The clocks move in whole timer ticks (about 10 ms), so every duration check
 * measures with CLOCK_MONOTONIC and allows for that. A signal that ends a
 * sleep comes from a child that waits SIGNAL_DELAY first: long enough for the
 * parent to be asleep, short enough that most of the sleep is left.
 */

#define NS_PER_SECOND 1000000000LL
#define NS_PER_MS 1000000LL
#define JAN_1_2024 1704067200 // seconds since 1970: the clock was set at boot
#define SHORT_SLEEP_MS 50
#define LONG_SLEEP_S 5
#define SIGNAL_DELAY_MS 100
#define NOT_USER_ADDRESS ((void *)0xFFFFFFFF80100000ULL)

static volatile sig_atomic_t handled;

static void on_signal(int signal)
{
    (void)signal;
    handled++;
}

static int64_t ns_of(struct timespec t)
{
    return (int64_t)t.tv_sec * NS_PER_SECOND + t.tv_nsec;
}

static int64_t monotonic_ns(void)
{
    struct timespec now;
    CHECK(clock_gettime(CLOCK_MONOTONIC, &now) == 0);
    return ns_of(now);
}

static void handle(int flags)
{
    struct sigaction action = {0};
    action.sa_handler = on_signal;
    action.sa_flags = flags;
    sigemptyset(&action.sa_mask);
    CHECK(sigaction(SIGUSR1, &action, NULL) == 0);
    handled = 0;
}

// A child that sends SIGUSR1 to this process after SIGNAL_DELAY_MS
static pid_t signal_later(void)
{
    pid_t target = getpid();
    fflush(stdout);
    pid_t pid = fork();
    CHECK(pid >= 0);
    if (pid == 0)
    {
        struct timespec delay = {.tv_sec = 0, .tv_nsec = SIGNAL_DELAY_MS * NS_PER_MS};
        nanosleep(&delay, NULL);
        kill(target, SIGUSR1);
        _exit(0);
    }
    return pid;
}

static void reap(pid_t pid)
{
    int status;
    while (waitpid(pid, &status, 0) < 0 && errno == EINTR)
        ;
}

// ---- Tests ----

static void test_clocks(void)
{
    struct timespec wall, a, b;

    CHECK(clock_gettime(CLOCK_REALTIME, &wall) == 0);
    CHECK(wall.tv_sec >= JAN_1_2024);
    CHECK(wall.tv_nsec >= 0 && wall.tv_nsec < NS_PER_SECOND);

    time_t stored = 0;
    time_t now = time(&stored);
    CHECK(now == stored);
    CHECK(now >= wall.tv_sec && now <= wall.tv_sec + 1);
    CHECK(time(NULL) >= now);

    CHECK(clock_gettime(CLOCK_MONOTONIC, &a) == 0);
    CHECK(a.tv_nsec >= 0 && a.tv_nsec < NS_PER_SECOND);
    CHECK(ns_of(a) > 0); // the timer has ticked since boot
    CHECK(clock_gettime(CLOCK_MONOTONIC, &b) == 0);
    CHECK(ns_of(b) >= ns_of(a));
}

static void test_clock_errors(void)
{
    struct timespec t;

    errno = 0;
    CHECK(clock_gettime(2, &t) == -1 && errno == EINVAL);
    errno = 0;
    CHECK(clock_gettime(-1, &t) == -1 && errno == EINVAL);
    errno = 0;
    CHECK(clock_gettime(CLOCK_MONOTONIC, NOT_USER_ADDRESS) == -1 && errno == EFAULT);
}

static void test_sleep_lasts(void)
{
    struct timespec request = {.tv_sec = 0, .tv_nsec = SHORT_SLEEP_MS * NS_PER_MS};
    struct timespec remaining = {.tv_sec = 99, .tv_nsec = 99};

    int64_t start = monotonic_ns();
    CHECK(nanosleep(&request, &remaining) == 0);
    int64_t slept = monotonic_ns() - start;

    CHECK(slept >= SHORT_SLEEP_MS * NS_PER_MS);
    CHECK(slept < 1 * NS_PER_SECOND); // not stuck
    CHECK(remaining.tv_sec == 99 && remaining.tv_nsec == 99); // untouched when it ran out

    // Whole seconds count too
    struct timespec one = {.tv_sec = 1, .tv_nsec = 0};
    start = monotonic_ns();
    CHECK(nanosleep(&one, NULL) == 0);
    CHECK(monotonic_ns() - start >= NS_PER_SECOND);

    // Zero: returns at once
    struct timespec zero = {0};
    CHECK(nanosleep(&zero, NULL) == 0);
    CHECK(sleep(0) == 0);
}

static void test_sleep_errors(void)
{
    struct timespec bad[] = {
        {.tv_sec = 0, .tv_nsec = NS_PER_SECOND},
        {.tv_sec = 0, .tv_nsec = -1},
        {.tv_sec = -1, .tv_nsec = 0},
    };

    for (size_t i = 0; i < sizeof(bad) / sizeof(bad[0]); i++)
    {
        errno = 0;
        CHECK(nanosleep(&bad[i], NULL) == -1 && errno == EINVAL);
    }

    errno = 0;
    CHECK(nanosleep(NOT_USER_ADDRESS, NULL) == -1 && errno == EFAULT);
}

// A handler ends the sleep: EINTR and the time left
static void test_signal_ends_sleep(int flags)
{
    handle(flags);
    pid_t sender = signal_later();

    struct timespec request = {.tv_sec = LONG_SLEEP_S, .tv_nsec = 0};
    struct timespec remaining = {0};

    int64_t start = monotonic_ns();
    errno = 0;
    int result = nanosleep(&request, &remaining);
    int64_t slept = monotonic_ns() - start;

    // Even with SA_RESTART: the caller gets the time left and decides (Linux too)
    CHECK(result == -1 && errno == EINTR);
    CHECK(handled == 1);
    CHECK(slept < LONG_SLEEP_S * NS_PER_SECOND);
    CHECK(remaining.tv_nsec >= 0 && remaining.tv_nsec < NS_PER_SECOND);
    CHECK(ns_of(remaining) > 0 && ns_of(remaining) < LONG_SLEEP_S * NS_PER_SECOND);

    // Slept + left is the request, give or take a tick or two
    int64_t total = slept + ns_of(remaining);
    CHECK(total >= LONG_SLEEP_S * NS_PER_SECOND - 50 * NS_PER_MS);
    CHECK(total <= LONG_SLEEP_S * NS_PER_SECOND + 50 * NS_PER_MS);

    reap(sender);
}

// sleep(): the whole seconds left, rounded up
static void test_signal_ends_sleep_seconds(void)
{
    handle(0);
    pid_t sender = signal_later();

    unsigned left = sleep(LONG_SLEEP_S);

    CHECK(handled == 1);
    CHECK(left == LONG_SLEEP_S); // 4.9 s left rounds up to 5

    reap(sender);
}

int main(void)
{
    test_clocks();
    test_clock_errors();
    test_sleep_lasts();
    test_sleep_errors();
    test_signal_ends_sleep(0);
    test_signal_ends_sleep(SA_RESTART);
    test_signal_ends_sleep_seconds();
    return test_status();
}
