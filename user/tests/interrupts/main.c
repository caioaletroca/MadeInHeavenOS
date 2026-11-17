#include <sys/mman.h>
#include <fcntl.h>
#include <stdint.h>
#include <time.h>
#include <unistd.h>
#include <test.h>

/*
 * System calls run with IRQs on: the timer keeps ticking through a long one.
 * With IRQs off for a whole call, the PIC keeps one timer interrupt pending
 * and drops the rest, so a 200 ms call moved the monotonic clock by one tick.
 *
 * The reference is the CPU's cycle counter (rdtsc works in ring 3), which no
 * interrupt drives. Calibrated during a sleep (IRQs on, no tick lost), it says
 * how long the long call really took; the monotonic clock must agree, within
 * a few ticks. The long call is one write of a big buffer to /dev/null: 256
 * bytes at a time, each copied in, each page mapped at its first read. Its
 * size doubles until one call lasts LONG_MS by the cycle counter.
 *
 * CPU time is sampled per tick too: the call is charged to the process.
 *
 * The slack grows with the call: an emulator starved by its host (one 40 ms
 * write took 5.7 s) drops timer ticks too, which no guest can tell from ticks
 * its kernel lost. A few percent covers that; IRQs off through the call lose
 * all but one tick, far past it.
 */

#define NS_PER_MS 1000000LL
#define CALIBRATION_MS 300
#define LONG_MS 150
#define SLACK_MS 30         // a few ticks: each reading of the clock is up to one off
#define SLACK_PER_CENT 5    // and ticks a starved emulator drops
#define FIRST_SIZE (8UL << 20)
#define LAST_SIZE (128UL << 20)

static uint64_t cycles(void)
{
    uint32_t low, high;
    __asm__ __volatile__("rdtsc" : "=a"(low), "=d"(high));
    return ((uint64_t)high << 32) | low;
}

static int64_t clock_ms(clockid_t clock)
{
    struct timespec now;
    CHECK(clock_gettime(clock, &now) == 0);
    return (int64_t)now.tv_sec * 1000 + now.tv_nsec / NS_PER_MS;
}

// Spins until the monotonic clock ticks: the start of a tick, read at once. Its value
static int64_t next_tick_ms(void)
{
    int64_t now = clock_ms(CLOCK_MONOTONIC);
    int64_t next;
    while ((next = clock_ms(CLOCK_MONOTONIC)) == now)
        ;
    return next;
}

/*
 * Cycles per millisecond, measured over a sleep from one tick's start to
 * another's: the clock moves in whole ticks, so readings at arbitrary moments
 * would each be up to a tick off (3% over 300 ms, 170 ms over a 5.7 s call).
 */
static uint64_t calibrate(void)
{
    struct timespec sleep = {0, CALIBRATION_MS * NS_PER_MS};
    int64_t start_ms = next_tick_ms();
    uint64_t start = cycles();
    CHECK(nanosleep(&sleep, NULL) == 0);
    int64_t elapsed_ms = next_tick_ms() - start_ms;
    uint64_t elapsed = cycles() - start;

    CHECK(elapsed_ms >= CALIBRATION_MS);
    return elapsed_ms > 0 ? elapsed / (uint64_t)elapsed_ms : 1;
}

int main(void)
{
    uint64_t per_ms = calibrate();
    CHECK(per_ms > 0);

    int fd = open("/dev/null", O_WRONLY);
    CHECK(fd >= 0);

    int checked = 0;
    for (size_t size = FIRST_SIZE; size <= LAST_SIZE && !checked; size *= 2)
    {
        char *buffer = mmap(NULL, size, PROT_READ, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        CHECK(buffer != MAP_FAILED);
        if (buffer == MAP_FAILED)
            break;

        int64_t start_ms = clock_ms(CLOCK_MONOTONIC);
        int64_t start_cpu_ms = clock_ms(CLOCK_PROCESS_CPUTIME_ID);
        uint64_t start = cycles();
        CHECK(write(fd, buffer, size) == (ssize_t)size);
        int64_t real_ms = (int64_t)((cycles() - start) / per_ms);
        int64_t monotonic_ms = clock_ms(CLOCK_MONOTONIC) - start_ms;
        int64_t cpu_ms = clock_ms(CLOCK_PROCESS_CPUTIME_ID) - start_cpu_ms;

        CHECK(munmap(buffer, size) == 0);

        if (real_ms < LONG_MS)
            continue; // too short to tell a lost tick from slack: twice the size

        checked = 1;
        int64_t slack = SLACK_MS + real_ms * SLACK_PER_CENT / 100;
        if (monotonic_ms < real_ms - slack || monotonic_ms > real_ms + slack || cpu_ms < real_ms - slack)
            printf("  %lu MiB: %ld ms by the cycle counter, %ld ms monotonic, %ld ms of CPU time\n",
                   (unsigned long)(size >> 20), (long)real_ms, (long)monotonic_ms, (long)cpu_ms);
        CHECK(monotonic_ms >= real_ms - slack && monotonic_ms <= real_ms + slack);
        CHECK(cpu_ms >= real_ms - slack);
    }

    if (!checked)
        printf("  no write lasted %d ms: not checked\n", LONG_MS);

    close(fd);
    return test_status();
}
