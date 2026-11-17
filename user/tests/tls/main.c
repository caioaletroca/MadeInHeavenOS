#include <sys/wait.h>
#include <mihos/syscall.h>
#include <errno.h>
#include <signal.h>
#include <stddef.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <test.h>

/*
 * The thread pointer (TLS): each thread's own FS base, set by a system call.
 * Code reaches its thread's block through %fs-relative loads, as compiled
 * __thread variables do. The kernel loads it when the thread switches in and
 * keeps it at fork (the child's memory is a copy).
 *
 * The libc owns FS: mlibc keeps its thread block there (errno, fork's
 * handlers), so a libc call made with the test's block in place reads
 * garbage. Each check that changes FS runs in a child that, from the change
 * on, makes only raw system calls (which never touch errno) and reports
 * through its exit status: 0, or the number of the check that failed.
 *
 * Not checked here: that exec clears it. A program's libc sets its own block
 * again at start-up before main runs, so main cannot see what exec left.
 */

#define SWITCH_ROUNDS 5
#define SLEEP_MS 20

// The x86-64 ABI's layout: the first word points to the block itself
typedef struct tcb
{
    struct tcb *self;
    long value;
} tcb_t;

// A system call with no libc around it: the kernel's result as it is (-errno on error)
static long raw_call(long number, long first, long second, long third)
{
    long result;
    __asm__ __volatile__("int $0x80"
                         : "=a"(result)
                         : "a"(number), "D"(first), "S"(second), "d"(third)
                         : "rcx", "r11", "memory");
    return result;
}

static long set_thread_pointer(void *pointer)
{
    return raw_call(SYS_SET_THREAD_POINTER, (long)pointer, 0, 0);
}

static __attribute__((noreturn)) void raw_exit(int status)
{
    raw_call(SYS_EXIT, status, 0, 0);
    __builtin_unreachable();
}

static void raw_sleep_ms(long ms)
{
    struct timespec t = {0, ms * 1000000L};
    raw_call(SYS_NANOSLEEP, (long)&t, 0, 0);
}

// A word at offset bytes from the thread pointer
static long fs_word(long offset)
{
    long value;
    __asm__ __volatile__("mov %%fs:(%1), %0" : "=r"(value) : "r"(offset) : "memory");
    return value;
}

static tcb_t *fs_self(void)
{
    return (tcb_t *)fs_word(0);
}

static long fs_value(void)
{
    return fs_word(offsetof(tcb_t, value));
}

// Runs check in a child (libc's fork: FS is still libc's) and returns its wait status
static int in_child(void (*check)(void))
{
    fflush(stdout);
    pid_t pid = fork();
    if (pid == 0)
        check(); // never returns
    int status = -1;
    CHECK(pid > 0 && waitpid(pid, &status, 0) == pid);
    return status;
}

static void expect_exit_0(const char *name, int status)
{
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0)
        printf("  %s: status %#x (exit status = the check that failed)\n", name, status);
    CHECK(WIFEXITED(status) && WEXITSTATUS(status) == 0);
}

// ---- Setting and reading ----

static tcb_t block = {&block, 111};

static void set_and_read(void)
{
    if (set_thread_pointer(&block) != 0)
        raw_exit(1);
    if (fs_self() != &block || fs_value() != 111)
        raw_exit(2);

    // Writes through the block show through FS: it is the same memory
    block.value = 112;
    if (fs_value() != 112)
        raw_exit(3);
    block.value = 111;

    // Kernel and non-canonical addresses are refused (wrmsr would fault in the kernel)
    if (set_thread_pointer((void *)0x0000800000000000UL) != -EINVAL) // USER_TOP
        raw_exit(4);
    if (set_thread_pointer((void *)0x8000000000000000UL) != -EINVAL) // not canonical
        raw_exit(5);
    if (set_thread_pointer((void *)0xFFFFFFFF80000000UL) != -EINVAL) // the kernel
        raw_exit(6);
    if (fs_self() != &block) // unchanged
        raw_exit(7);
    raw_exit(0);
}

// 0 clears it: %fs:0 then reads address 0
static void cleared_faults(void)
{
    if (set_thread_pointer(NULL) != 0)
        raw_exit(1);
    fs_self();
    raw_exit(0); // not reached
}

// ---- Context switches and fork ----

static tcb_t outer = {&outer, 111};
static tcb_t inner = {&inner, 222};

/*
 * Each side keeps its own pointer across many switches; the inner child starts
 * with the outer's (inherited at fork) and its change stays its own.
 */
static void switches(void)
{
    if (set_thread_pointer(&outer) != 0)
        raw_exit(1);

    long pid = raw_call(SYS_FORK, 0, 0, 0);
    if (pid < 0)
        raw_exit(2);
    if (pid == 0)
    {
        if (fs_self() != &outer || fs_value() != 111) // inherited at fork
            raw_exit(10);
        if (set_thread_pointer(&inner) != 0)
            raw_exit(11);
        for (int round = 0; round < SWITCH_ROUNDS; round++)
        {
            raw_sleep_ms(SLEEP_MS);
            if (fs_self() != &inner || fs_value() != 222)
                raw_exit(12);
        }
        raw_exit(0);
    }

    for (int round = 0; round < SWITCH_ROUNDS; round++)
    {
        raw_sleep_ms(SLEEP_MS);
        if (fs_self() != &outer || fs_value() != 111)
            raw_exit(3);
    }

    int status = -1;
    if (raw_call(SYS_WAIT, pid, (long)&status, 0) != pid)
        raw_exit(4);
    if (!WIFEXITED(status))
        raw_exit(5);
    if (WEXITSTATUS(status) != 0)
        raw_exit(WEXITSTATUS(status)); // the inner child's check
    if (fs_self() != &outer) // the inner child's change stayed in the inner child
        raw_exit(6);
    raw_exit(0);
}

// ---- Signal handlers ----

static volatile long seen_in_handler;

// Raw only: it runs with the test's block in place
static void on_signal(int signal)
{
    (void)signal;
    seen_in_handler = fs_value();
}

// A handler runs on the same thread: the same block
static void handler_sees_block(void)
{
    if (set_thread_pointer(&block) != 0)
        raw_exit(1);
    seen_in_handler = 0;
    long self = raw_call(SYS_GETPID, 0, 0, 0);
    if (raw_call(SYS_KILL, self, SIGUSR1, 0) != 0)
        raw_exit(2);
    if (seen_in_handler != 111)
        raw_exit(3);
    raw_exit(0);
}

int main(void)
{
    expect_exit_0("set and read", in_child(set_and_read));

    int status = in_child(cleared_faults);
    CHECK(WIFSIGNALED(status) && WTERMSIG(status) == SIGSEGV);

    expect_exit_0("switches", in_child(switches));

    // Installed with libc while FS is still libc's; the child inherits it at fork
    struct sigaction action = {0};
    action.sa_handler = on_signal;
    sigemptyset(&action.sa_mask);
    CHECK(sigaction(SIGUSR1, &action, NULL) == 0);
    expect_exit_0("signal handler", in_child(handler_sees_block));

    return test_status();
}
