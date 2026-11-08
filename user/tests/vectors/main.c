#include <signal.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <mihos/syscall.h>
#include <test.h>

/*
 * The vector registers (xmm0-xmm15) and MXCSR belong to each program, like
 * the general ones: the kernel stores them when it switches threads, copies
 * them into a fork child, keeps them in a signal frame, and starts a new
 * program (exec) and a signal handler with the reset values.
 *
 * Every check fills all sixteen xmm registers, makes one system call and reads
 * them back in a single asm block, so no compiled code can touch them in
 * between: whatever changed them was the kernel or a handler.
 *
 * argv[1] is "exec" when the test runs itself through exec.
 */

#define SELF_PATH "/usr/tests/vectors"
#define SWITCH_ROUNDS 200
#define VECTOR_BYTES 256 // 16 registers of 16 bytes

#define MXCSR_DEFAULT 0x1F80    // every exception masked, round to nearest
#define MXCSR_TOWARD_ZERO 0x7F80 // the same, rounding toward zero
#define MXCSR_FLAGS 0x3F         // exceptions seen so far: any float code may set them

static volatile int handler_saw_clean;

static unsigned mxcsr_get(void)
{
    unsigned value;
    __asm__ __volatile__("stmxcsr %0" : "=m"(value));
    return value & ~MXCSR_FLAGS;
}

static void mxcsr_set(unsigned value)
{
    __asm__ __volatile__("ldmxcsr %0" : : "m"(value));
}

static void pattern_fill(uint8_t bytes[VECTOR_BYTES], uint8_t seed)
{
    for (int i = 0; i < VECTOR_BYTES; i++)
        bytes[i] = (uint8_t)(seed + i);
}

/*
 * Loads xmm0-xmm15 from `in`, makes system call `number` with arguments a
 * and b, and stores xmm0-xmm15 into `out`. Returns the call's result.
 */
static long vectors_around_syscall(const uint8_t in[VECTOR_BYTES], uint8_t out[VECTOR_BYTES],
                                   long number, long a, long b)
{
    long result = number;
    __asm__ __volatile__(
        "movdqu 0(%[in]), %%xmm0\n\t"
        "movdqu 16(%[in]), %%xmm1\n\t"
        "movdqu 32(%[in]), %%xmm2\n\t"
        "movdqu 48(%[in]), %%xmm3\n\t"
        "movdqu 64(%[in]), %%xmm4\n\t"
        "movdqu 80(%[in]), %%xmm5\n\t"
        "movdqu 96(%[in]), %%xmm6\n\t"
        "movdqu 112(%[in]), %%xmm7\n\t"
        "movdqu 128(%[in]), %%xmm8\n\t"
        "movdqu 144(%[in]), %%xmm9\n\t"
        "movdqu 160(%[in]), %%xmm10\n\t"
        "movdqu 176(%[in]), %%xmm11\n\t"
        "movdqu 192(%[in]), %%xmm12\n\t"
        "movdqu 208(%[in]), %%xmm13\n\t"
        "movdqu 224(%[in]), %%xmm14\n\t"
        "movdqu 240(%[in]), %%xmm15\n\t"
        "int $0x80\n\t"
        "movdqu %%xmm0, 0(%[out])\n\t"
        "movdqu %%xmm1, 16(%[out])\n\t"
        "movdqu %%xmm2, 32(%[out])\n\t"
        "movdqu %%xmm3, 48(%[out])\n\t"
        "movdqu %%xmm4, 64(%[out])\n\t"
        "movdqu %%xmm5, 80(%[out])\n\t"
        "movdqu %%xmm6, 96(%[out])\n\t"
        "movdqu %%xmm7, 112(%[out])\n\t"
        "movdqu %%xmm8, 128(%[out])\n\t"
        "movdqu %%xmm9, 144(%[out])\n\t"
        "movdqu %%xmm10, 160(%[out])\n\t"
        "movdqu %%xmm11, 176(%[out])\n\t"
        "movdqu %%xmm12, 192(%[out])\n\t"
        "movdqu %%xmm13, 208(%[out])\n\t"
        "movdqu %%xmm14, 224(%[out])\n\t"
        "movdqu %%xmm15, 240(%[out])"
        : "+a"(result)
        : [in] "r"(in), [out] "r"(out), "D"(a), "S"(b)
        : "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6", "xmm7",
          "xmm8", "xmm9", "xmm10", "xmm11", "xmm12", "xmm13", "xmm14", "xmm15", "memory");
    return result;
}

// One process's half of the switch test: its own values, yielding to the other every round
static void yield_rounds(uint8_t seed)
{
    uint8_t in[VECTOR_BYTES], out[VECTOR_BYTES];
    pattern_fill(in, seed);

    for (int i = 0; i < SWITCH_ROUNDS; i++)
    {
        vectors_around_syscall(in, out, SYS_YIELD, 0, 0);
        if (memcmp(in, out, VECTOR_BYTES) != 0)
        {
            CHECK(!"another process's vector registers showed up after a switch");
            return;
        }
    }
}

static void child_passed(pid_t pid)
{
    int status = -1;
    CHECK(waitpid(pid, &status, 0) == pid);
    CHECK(WIFEXITED(status) && WEXITSTATUS(status) == 0);
}

// Two processes with different values take turns on the CPU
static void test_switch(void)
{
    fflush(stdout);
    pid_t pid = fork();
    CHECK(pid >= 0);
    if (pid == 0)
    {
        test_failures = 0;
        yield_rounds(0xA0);
        fflush(stdout);
        _exit(test_status());
    }

    yield_rounds(0x10);
    child_passed(pid);
}

// The child starts with the parent's registers, MXCSR included
static void test_fork(void)
{
    uint8_t in[VECTOR_BYTES], out[VECTOR_BYTES];
    pattern_fill(in, 0x33);

    fflush(stdout);
    mxcsr_set(MXCSR_TOWARD_ZERO);
    long pid = vectors_around_syscall(in, out, SYS_FORK, 0, 0);
    unsigned mxcsr = mxcsr_get();
    mxcsr_set(MXCSR_DEFAULT);

    CHECK(pid >= 0);
    if (pid == 0)
    {
        test_failures = 0;
        CHECK(memcmp(in, out, VECTOR_BYTES) == 0);
        CHECK(mxcsr == MXCSR_TOWARD_ZERO);
        fflush(stdout);
        _exit(test_status());
    }

    CHECK(memcmp(in, out, VECTOR_BYTES) == 0);
    CHECK(mxcsr == MXCSR_TOWARD_ZERO);
    child_passed((pid_t)pid);
}

// Checks it was entered with the reset values, then leaves nothing of them
static void on_signal_trash(int signal)
{
    (void)signal;
    uint64_t low;
    unsigned mxcsr;

    __asm__ __volatile__("movq %%xmm15, %0\n\tstmxcsr %1" : "=r"(low), "=m"(mxcsr));
    handler_saw_clean = low == 0 && (mxcsr & ~MXCSR_FLAGS) == MXCSR_DEFAULT;

    unsigned round_down = 0x3F80;
    __asm__ __volatile__(
        "pcmpeqd %%xmm0, %%xmm0\n\tpcmpeqd %%xmm1, %%xmm1\n\tpcmpeqd %%xmm2, %%xmm2\n\t"
        "pcmpeqd %%xmm3, %%xmm3\n\tpcmpeqd %%xmm4, %%xmm4\n\tpcmpeqd %%xmm5, %%xmm5\n\t"
        "pcmpeqd %%xmm6, %%xmm6\n\tpcmpeqd %%xmm7, %%xmm7\n\tpcmpeqd %%xmm8, %%xmm8\n\t"
        "pcmpeqd %%xmm9, %%xmm9\n\tpcmpeqd %%xmm10, %%xmm10\n\tpcmpeqd %%xmm11, %%xmm11\n\t"
        "pcmpeqd %%xmm12, %%xmm12\n\tpcmpeqd %%xmm13, %%xmm13\n\tpcmpeqd %%xmm14, %%xmm14\n\t"
        "pcmpeqd %%xmm15, %%xmm15\n\tldmxcsr %0"
        :
        : "m"(round_down)
        : "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6", "xmm7",
          "xmm8", "xmm9", "xmm10", "xmm11", "xmm12", "xmm13", "xmm14", "xmm15");
}

// The handler runs on the way out of kill; sigreturn must bring every value back
static void test_signal(void)
{
    struct sigaction action = {0};
    action.sa_handler = on_signal_trash;
    sigemptyset(&action.sa_mask);
    CHECK(sigaction(SIGUSR1, &action, NULL) == 0);

    uint8_t in[VECTOR_BYTES], out[VECTOR_BYTES];
    pattern_fill(in, 0x77);
    long self = getpid();

    mxcsr_set(MXCSR_TOWARD_ZERO);
    long result = vectors_around_syscall(in, out, SYS_KILL, self, SIGUSR1);
    unsigned mxcsr = mxcsr_get();
    mxcsr_set(MXCSR_DEFAULT);

    CHECK(result == 0);
    CHECK(handler_saw_clean);
    CHECK(memcmp(in, out, VECTOR_BYTES) == 0);
    CHECK(mxcsr == MXCSR_TOWARD_ZERO);

    action.sa_handler = SIG_DFL;
    CHECK(sigaction(SIGUSR1, &action, NULL) == 0);
}

// A new program gets the reset MXCSR, not the one exec was called with
static void test_exec(void)
{
    fflush(stdout);
    pid_t pid = fork();
    CHECK(pid >= 0);
    if (pid == 0)
    {
        char *argv[] = {SELF_PATH, "exec", NULL};
        mxcsr_set(MXCSR_TOWARD_ZERO);
        execv(SELF_PATH, argv);
        _exit(127);
    }

    child_passed(pid);
}

int main(int argc, char **argv)
{
    if (argc > 1 && strcmp(argv[1], "exec") == 0)
        return mxcsr_get() == MXCSR_DEFAULT ? 0 : 1;

    test_switch();
    test_fork();
    test_signal();
    test_exec();
    return test_status();
}
