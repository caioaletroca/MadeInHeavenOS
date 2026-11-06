#include <sys/wait.h>
#include <unistd.h>
#include <signal.h>
#include <spawn.h>
#include <stdint.h>
#include <errno.h>
#include <mihos/syscall.h>
#include <test.h>

/*
 * Signal handlers: sigaction, sigprocmask and sigreturn. The kernel pauses the
 * program, runs its handler on the program's own stack, and sigreturn puts
 * every register back. Each test runs in a child of its own, so what it does
 * to its signals ends with it; a child counts its own failures and reports
 * them through its exit status.
 *
 * A sender cannot know when its target has fallen asleep, so a helper process
 * (the "poker") sends SIGUSR1 over and over until it is stopped. With a
 * handler, a signal that arrives too early just runs it, and the next one
 * finds the sleep.
 *
 * argv[1] is set when the test runs itself through exec or spawn.
 */

#define SELF_PATH "/usr/tests/sigaction"
#define POKE_SPINS 10000
#define REGISTER_ROUNDS 20 // signals landing on interrupted code
#define MIX_ROUNDS 1000
#define RED_ZONE_ROUNDS 5
#define BIG_HANDLER_STACK (256 * 1024) // past the pages the stack has mapped
#define STREAM_SIZE 16384              // several pipes' worth, so writes sleep
#define READER_SPINS 1000000
#define FAULT_EXIT 7
#define NOT_USER_ADDRESS 0xFFFFFFFF80100000ULL
#define NOT_CANONICAL 0x4141414141414141ULL
#define JUNK_WORDS 256

static volatile sig_atomic_t count;
static volatile sig_atomic_t last_signal;
static volatile int handler_aligned;
static volatile int steps[8];
static volatile int step_count;

static char *child_environ[] = {NULL};

static pid_t fork_flushed(void)
{
    fflush(stdout);
    pid_t pid = fork();
    if (pid == 0)
        test_failures = 0; // a child reports only its own
    return pid;
}

// Leaves the child: exit status = its failed checks. _exit, so atexit never runs twice
static void child_done(void)
{
    fflush(stdout);
    _exit(test_status());
}

static void child_passed(pid_t pid)
{
    int status = -1;
    CHECK(waitpid(pid, &status, 0) == pid);
    CHECK(WIFEXITED(status) && WEXITSTATUS(status) == 0);
}

static void child_killed_by(pid_t pid, int signal)
{
    int status = 0;
    CHECK(waitpid(pid, &status, 0) == pid);
    CHECK(WIFSIGNALED(status) && WTERMSIG(status) == signal);
}

static void child_exited_with(pid_t pid, int expected)
{
    int status = -1;
    CHECK(waitpid(pid, &status, 0) == pid);
    CHECK(WIFEXITED(status) && WEXITSTATUS(status) == expected);
}

// Runs a test in a child of its own
static void isolated(void (*test)(void))
{
    pid_t child = fork_flushed();
    CHECK(child >= 0);
    if (child == 0)
    {
        test();
        child_done();
    }
    child_passed(child);
}

static sigset_t set_of(int signal)
{
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, signal);
    return set;
}

static void handle(int signal, void (*handler)(int), sigset_t mask)
{
    struct sigaction action = {0};
    action.sa_handler = handler;
    action.sa_mask = mask;
    CHECK(sigaction(signal, &action, NULL) == 0);
}

static void handle_plain(int signal, void (*handler)(int))
{
    sigset_t empty;
    sigemptyset(&empty);
    handle(signal, handler, empty);
}

static void block(int signal)
{
    sigset_t set = set_of(signal);
    CHECK(sigprocmask(SIG_BLOCK, &set, NULL) == 0);
}

static void unblock(int signal)
{
    sigset_t set = set_of(signal);
    CHECK(sigprocmask(SIG_UNBLOCK, &set, NULL) == 0);
}

static void step(int value)
{
    steps[step_count++] = value;
}

static void expect_steps(int a, int b, int c)
{
    CHECK(step_count == 3);
    CHECK(steps[0] == a && steps[1] == b && steps[2] == c);
    step_count = 0;
}

// ---- Handlers ----

static void on_signal(int signal)
{
    count++;
    last_signal = signal;
}

// A frame pointer is 16-aligned when the entry stack was as a `call` leaves it
static void on_signal_aligned(int signal)
{
    (void)signal;
    handler_aligned = ((uintptr_t)__builtin_frame_address(0) % 16) == 0;
}

// Leaves nothing the interrupted code could have relied on
static void on_signal_trash(int signal)
{
    (void)signal;
    count++;
    __asm__ __volatile__(
        "movq $-1, %%rax\n\tmovq $-1, %%rbx\n\tmovq $-1, %%rcx\n\tmovq $-1, %%rdx\n\t"
        "movq $-1, %%rsi\n\tmovq $-1, %%rdi\n\tmovq $-1, %%r8\n\tmovq $-1, %%r9\n\t"
        "movq $-1, %%r10\n\tmovq $-1, %%r11\n\tmovq $-1, %%r12\n\tmovq $-1, %%r13\n\t"
        "movq $-1, %%r14\n\tmovq $-1, %%r15\n\tcmpq %%rax, %%rax"
        : : : "rax", "rbx", "rcx", "rdx", "rsi", "rdi", "r8", "r9", "r10", "r11",
          "r12", "r13", "r14", "r15", "cc", "memory");
}

static void on_signal_big(int signal)
{
    (void)signal;
    volatile char big[BIG_HANDLER_STACK];
    big[0] = 1;
    big[BIG_HANDLER_STACK - 1] = 2;
    count = big[0] + big[BIG_HANDLER_STACK - 1];
}

static void on_outer(int signal)
{
    (void)signal;
    step(1);
    raise(SIGUSR2);
    step(3);
}

static void on_inner(int signal)
{
    (void)signal;
    step(2);
}

static void on_again(int signal)
{
    (void)signal;
    if (count++ == 0)
    {
        step(1);
        raise(SIGUSR1); // blocked here: it has to wait for this handler's end
        step(2);
    }
    else
    {
        step(3);
    }
}

static void on_fault_exit(int signal)
{
    (void)signal;
    _exit(FAULT_EXIT);
}

// ---- The poker ----

static pid_t poker_start(void)
{
    pid_t target = getpid();
    pid_t poker = fork_flushed();
    CHECK(poker >= 0);
    if (poker == 0)
    {
        for (;;)
        {
            kill(target, SIGUSR1);
            for (volatile int i = 0; i < POKE_SPINS; i++)
                ;
        }
    }
    return poker;
}

static void poker_stop(pid_t poker)
{
    kill(poker, SIGKILL);
    // A poke sent before it died may still end this wait early
    while (waitpid(poker, NULL, 0) < 0 && errno == EINTR)
        ;
}

// ---- Tests ----

static void test_handler_runs(void)
{
    handle_plain(SIGUSR1, on_signal);

    // On the way out of kill, so it already ran when kill returns, and kill still returns 0
    CHECK(raise(SIGUSR1) == 0);
    CHECK(count == 1 && last_signal == SIGUSR1);
    CHECK(kill(getpid(), SIGUSR1) == 0);
    CHECK(count == 2);

    // Any fatal signal can be caught, with the same handler for several
    handle_plain(SIGTERM, on_signal);
    CHECK(raise(SIGTERM) == 0);
    CHECK(count == 3 && last_signal == SIGTERM);

    // Even one that does nothing by default
    handle_plain(SIGCHLD, on_signal);
    CHECK(raise(SIGCHLD) == 0);
    CHECK(count == 4 && last_signal == SIGCHLD);
}

static void test_alignment(void)
{
    handle_plain(SIGUSR1, on_signal_aligned);
    raise(SIGUSR1);
    CHECK(handler_aligned);
}

static void test_old_action(void)
{
    struct sigaction old;

    CHECK(sigaction(SIGUSR1, NULL, &old) == 0);
    CHECK(old.sa_handler == SIG_DFL);

    sigset_t mask = set_of(SIGUSR2);
    handle(SIGUSR1, on_signal, mask);

    struct sigaction ignore = {0};
    ignore.sa_handler = SIG_IGN;
    CHECK(sigaction(SIGUSR1, &ignore, &old) == 0);
    CHECK(old.sa_handler == on_signal && sigismember(&old.sa_mask, SIGUSR2) == 1);

    CHECK(sigaction(SIGUSR1, NULL, &old) == 0);
    CHECK(old.sa_handler == SIG_IGN);
}

static void test_ignore(void)
{
    struct sigaction action = {0};
    action.sa_handler = SIG_IGN;
    CHECK(sigaction(SIGTERM, &action, NULL) == 0);
    CHECK(raise(SIGTERM) == 0);

    // Ignoring throws away what was waiting
    block(SIGUSR1);
    raise(SIGUSR1);
    CHECK(sigaction(SIGUSR1, &action, NULL) == 0);
    action.sa_handler = SIG_DFL;
    CHECK(sigaction(SIGUSR1, &action, NULL) == 0);
    unblock(SIGUSR1);

    // Back to the default, and the default ends the process
    pid_t child = fork_flushed();
    CHECK(child >= 0);
    if (child == 0)
    {
        CHECK(sigaction(SIGTERM, &action, NULL) == 0);
        raise(SIGTERM);
        _exit(1); // not reached
    }
    child_killed_by(child, SIGTERM);
}

static void test_errors(void)
{
    struct sigaction action = {0};
    action.sa_handler = on_signal;
    struct sigaction old;

    errno = 0;
    CHECK(sigaction(0, &action, NULL) == -1 && errno == EINVAL);
    errno = 0;
    CHECK(sigaction(-1, &action, NULL) == -1 && errno == EINVAL);
    errno = 0;
    CHECK(sigaction(NSIG, &action, NULL) == -1 && errno == EINVAL);

    // These two cannot be caught, ignored or blocked, but they can be asked about
    errno = 0;
    CHECK(sigaction(SIGKILL, &action, NULL) == -1 && errno == EINVAL);
    errno = 0;
    CHECK(sigaction(SIGSTOP, &action, NULL) == -1 && errno == EINVAL);
    CHECK(sigaction(SIGKILL, NULL, &old) == 0 && old.sa_handler == SIG_DFL);

    errno = 0;
    CHECK(sigaction(SIGUSR1, NULL, (struct sigaction *)16) == -1 && errno == EFAULT);

    sigset_t set;
    sigemptyset(&set);
    errno = 0;
    CHECK(sigprocmask(99, &set, NULL) == -1 && errno == EINVAL);
    errno = 0;
    CHECK(sigprocmask(SIG_SETMASK, (sigset_t *)16, NULL) == -1 && errno == EFAULT);
    errno = 0;
    CHECK(sigprocmask(SIG_SETMASK, &set, (sigset_t *)16) == -1 && errno == EFAULT);

    errno = 0;
    CHECK(sigaddset(&set, 0) == -1 && errno == EINVAL);
    errno = 0;
    CHECK(sigaddset(&set, NSIG) == -1 && errno == EINVAL);
    errno = 0;
    CHECK(sigdelset(&set, NSIG) == -1 && errno == EINVAL);
    errno = 0;
    CHECK(sigismember(&set, 0) == -1 && errno == EINVAL);
}

static void test_set_functions(void)
{
    sigset_t set;
    CHECK(sigemptyset(&set) == 0);
    CHECK(sigismember(&set, SIGINT) == 0);
    CHECK(sigaddset(&set, SIGINT) == 0);
    CHECK(sigismember(&set, SIGINT) == 1 && sigismember(&set, SIGTERM) == 0);
    CHECK(sigdelset(&set, SIGINT) == 0);
    CHECK(sigismember(&set, SIGINT) == 0);
    CHECK(sigfillset(&set) == 0);
    CHECK(sigismember(&set, SIGTERM) == 1 && sigismember(&set, 31) == 1);
}

static void test_mask(void)
{
    handle_plain(SIGUSR1, on_signal);

    // Waits while blocked, runs on the way out of the call that unblocks it
    block(SIGUSR1);
    raise(SIGUSR1);
    raise(SIGUSR1);
    CHECK(count == 0);
    unblock(SIGUSR1);
    CHECK(count == 1); // bits, not a queue: two sent, one run

    sigset_t old, set = set_of(SIGUSR2);
    CHECK(sigprocmask(SIG_SETMASK, &set, &old) == 0);
    CHECK(sigismember(&old, SIGUSR2) == 0);
    CHECK(sigprocmask(SIG_BLOCK, NULL, &old) == 0);
    CHECK(sigismember(&old, SIGUSR2) == 1);
}

// Nothing can stop these two: a process that could refuse them could never be stopped
static void test_unblockable(void)
{
    sigset_t all, now;
    sigfillset(&all);
    CHECK(sigprocmask(SIG_SETMASK, &all, NULL) == 0);
    CHECK(sigprocmask(SIG_BLOCK, NULL, &now) == 0);
    CHECK(sigismember(&now, SIGKILL) == 0 && sigismember(&now, SIGSTOP) == 0);
    CHECK(sigismember(&now, SIGTERM) == 1);

    pid_t child = fork_flushed();
    CHECK(child >= 0);
    if (child == 0)
    {
        raise(SIGKILL);
        _exit(1); // not reached
    }
    child_killed_by(child, SIGKILL);
}

static void test_nesting(void)
{
    // Not in sa_mask: the second handler runs inside the first
    handle_plain(SIGUSR1, on_outer);
    handle_plain(SIGUSR2, on_inner);
    raise(SIGUSR1);
    expect_steps(1, 2, 3);

    // In sa_mask: it waits for the first to end
    handle(SIGUSR1, on_outer, set_of(SIGUSR2));
    raise(SIGUSR1);
    expect_steps(1, 3, 2);

    // A handler's own signal is blocked while it runs
    handle_plain(SIGUSR1, on_again);
    raise(SIGUSR1);
    expect_steps(1, 2, 3);

    // sigreturn gave the mask back
    sigset_t now;
    CHECK(sigprocmask(SIG_BLOCK, NULL, &now) == 0);
    CHECK(sigismember(&now, SIGUSR1) == 0 && sigismember(&now, SIGUSR2) == 0);
}

// Every value the loop keeps in a register has to survive a signal between any two instructions
static uint64_t mix(void)
{
    uint64_t a = 1, b = 2, c = 3, d = 4, e = 5, f = 6, g = 7, h = 8;
    for (uint64_t i = 0; i < MIX_ROUNDS; i++)
    {
        a += h ^ i;
        b ^= (a << 1) + c;
        c += b ^ (d >> 1);
        d ^= c + e;
        e += d ^ (f << 2);
        f ^= e + g;
        g += f ^ (h >> 2);
        h ^= g + a;
    }
    return a ^ b ^ c ^ d ^ e ^ f ^ g ^ h;
}

static void test_registers(void)
{
    uint64_t expected = mix();
    handle_plain(SIGUSR1, on_signal_trash);

    pid_t poker = poker_start();
    while (count < REGISTER_ROUNDS)
    {
        if (mix() != expected)
        {
            CHECK(!"a register changed under a signal");
            break;
        }
    }
    poker_stop(poker);
}

// Leaf code may keep data in the 128 bytes below rsp without moving rsp, and a signal may land there
__attribute__((noinline)) static int red_zone_survives(void)
{
    uint64_t pattern = 0x1122334455667788ULL, low, high;
    int seen = count;

    __asm__ __volatile__(
        "movq %[pattern], -8(%%rsp)\n\t"
        "movq %[pattern], -128(%%rsp)\n\t"
        "1:\n\t"
        "cmpl %[seen], %[count]\n\t"
        "je 1b\n\t"
        "movq -8(%%rsp), %[high]\n\t"
        "movq -128(%%rsp), %[low]"
        : [high] "=&r"(high), [low] "=&r"(low)
        : [pattern] "r"(pattern), [seen] "r"(seen), [count] "m"(count)
        : "cc", "memory");

    return high == pattern && low == pattern;
}

static void test_red_zone(void)
{
    handle_plain(SIGUSR1, on_signal_trash);

    pid_t poker = poker_start();
    for (int i = 0; i < RED_ZONE_ROUNDS; i++)
        CHECK(red_zone_survives());
    poker_stop(poker);
}

// The frame and the handler both land on stack that may need growing
static void test_big_handler(void)
{
    handle_plain(SIGUSR1, on_signal_big);
    raise(SIGUSR1);
    CHECK(count == 3);
}

// A signal ended the sleep and the handler ran: the system call fails with EINTR
static void test_eintr_read(void)
{
    handle_plain(SIGUSR1, on_signal);
    int fds[2];
    CHECK(pipe(fds) == 0);

    pid_t poker = poker_start();
    char byte;
    errno = 0;
    CHECK(read(fds[0], &byte, 1) == -1 && errno == EINTR);
    CHECK(count >= 1);
    poker_stop(poker);
}

static void test_eintr_write(void)
{
    static const char data[STREAM_SIZE] = {0};
    handle_plain(SIGUSR1, on_signal);
    int fds[2];
    CHECK(pipe(fds) == 0);

    pid_t poker = poker_start();

    // Part of it went into the pipe: the count of that, not an error
    ssize_t written = write(fds[1], data, sizeof(data));
    CHECK(written > 0 && written < (ssize_t)sizeof(data));

    // Pipe full, nothing can go in: now the error
    errno = 0;
    CHECK(write(fds[1], data, sizeof(data)) == -1 && errno == EINTR);
    poker_stop(poker);
}

static void test_eintr_wait(void)
{
    handle_plain(SIGUSR1, on_signal);
    int fds[2];
    CHECK(pipe(fds) == 0);

    pid_t sleeper = fork_flushed();
    CHECK(sleeper >= 0);
    if (sleeper == 0)
    {
        char byte;
        read(fds[0], &byte, 1); // never gets data: it holds the write end too
        _exit(0);
    }

    pid_t poker = poker_start();
    errno = 0;
    CHECK(waitpid(sleeper, NULL, 0) == -1 && errno == EINTR);
    poker_stop(poker);

    // Nothing was lost: it is still there to be waited for
    CHECK(kill(sleeper, SIGKILL) == 0);
    child_killed_by(sleeper, SIGKILL);
}

// stdio retries an interrupted write: every byte arrives
static void test_stdio_retries(void)
{
    static char data[STREAM_SIZE];
    handle_plain(SIGUSR1, on_signal);
    int fds[2];
    CHECK(pipe(fds) == 0);

    pid_t reader = fork_flushed();
    CHECK(reader >= 0);
    if (reader == 0)
    {
        close(fds[1]);
        for (volatile int i = 0; i < READER_SPINS; i++)
            ;

        size_t total = 0;
        ssize_t n;
        while ((n = read(fds[0], data, sizeof(data))) > 0)
            total += (size_t)n;
        _exit(total == sizeof(data) ? 0 : 1);
    }
    close(fds[0]);

    int saved = dup(1);
    CHECK(saved >= 0);
    CHECK(dup2(fds[1], 1) == 1);
    close(fds[1]);

    pid_t poker = poker_start();
    size_t written = fwrite(data, 1, sizeof(data), stdout);
    int flushed = fflush(stdout);
    int failed = ferror(stdout);
    poker_stop(poker);

    dup2(saved, 1);
    close(saved);
    CHECK(written == sizeof(data) && flushed == 0 && !failed);
    child_passed(reader);
}

static void test_fork(void)
{
    handle_plain(SIGUSR1, on_signal);
    block(SIGUSR1);
    raise(SIGUSR1);

    pid_t child = fork_flushed();
    CHECK(child >= 0);
    if (child == 0)
    {
        // The mask and the handler come along; the waiting signal stays with the parent
        sigset_t now;
        CHECK(sigprocmask(SIG_BLOCK, NULL, &now) == 0);
        CHECK(sigismember(&now, SIGUSR1) == 1);
        unblock(SIGUSR1);
        CHECK(count == 0);
        raise(SIGUSR1);
        CHECK(count == 1);
        child_done();
    }
    child_passed(child);

    unblock(SIGUSR1);
    CHECK(count == 1);
}

// What exec and spawn have to hand over: see check_inherited
static void set_up_inherited(void)
{
    handle_plain(SIGUSR1, on_signal);
    struct sigaction ignore = {0};
    ignore.sa_handler = SIG_IGN;
    CHECK(sigaction(SIGUSR2, &ignore, NULL) == 0);
    block(SIGTERM);
    raise(SIGTERM);
}

// A handler lives in the old program, so it goes. Ignoring and the mask stay
static void check_inherited(void)
{
    struct sigaction old;
    CHECK(sigaction(SIGUSR1, NULL, &old) == 0 && old.sa_handler == SIG_DFL);
    CHECK(sigaction(SIGUSR2, NULL, &old) == 0 && old.sa_handler == SIG_IGN);

    sigset_t now;
    CHECK(sigprocmask(SIG_BLOCK, NULL, &now) == 0);
    CHECK(sigismember(&now, SIGTERM) == 1);
    raise(SIGUSR2); // survives
}

static void test_exec(void)
{
    pid_t child = fork_flushed();
    CHECK(child >= 0);
    if (child == 0)
    {
        set_up_inherited();
        char *argv[] = {SELF_PATH, "after-exec", NULL};
        execv(SELF_PATH, argv);
        _exit(1); // not reached
    }
    child_killed_by(child, SIGTERM);
}

static void test_spawn(void)
{
    set_up_inherited();

    char *argv[] = {SELF_PATH, "after-spawn", NULL};
    pid_t child;
    CHECK(posix_spawn(&child, SELF_PATH, NULL, NULL, argv, child_environ) == 0);
    child_passed(child);
}

// After exec: the same process, so SIGTERM still waits, and unblocking it ends us
static int after_exec(void)
{
    check_inherited();
    if (test_status() != 0)
        return test_status();
    unblock(SIGTERM);
    return 1; // not reached
}

// After spawn: a new process, so nothing waits
static int after_spawn(void)
{
    check_inherited();
    unblock(SIGTERM);
    return test_status();
}

static void fault_write(void)
{
    *(volatile int *)16 = 1;
}

static void fault_divide(void)
{
    volatile int zero = 0;
    volatile int result = 1 / zero;
    (void)result;
}

// A fault goes to the program's handler, like any signal
static void test_fault_handled(void)
{
    pid_t child = fork_flushed();
    CHECK(child >= 0);
    if (child == 0)
    {
        handle_plain(SIGSEGV, on_fault_exit);
        fault_write();
        _exit(1); // not reached
    }
    child_exited_with(child, FAULT_EXIT);

    child = fork_flushed();
    CHECK(child >= 0);
    if (child == 0)
    {
        handle_plain(SIGFPE, on_fault_exit);
        fault_divide();
        _exit(1); // not reached
    }
    child_exited_with(child, FAULT_EXIT);
}

// The instruction would run into the same fault forever, so nobody gets to refuse it
static void test_fault_not_refused(void)
{
    pid_t child = fork_flushed();
    CHECK(child >= 0);
    if (child == 0)
    {
        handle_plain(SIGSEGV, on_fault_exit);
        block(SIGSEGV);
        fault_write();
        _exit(1); // not reached
    }
    child_killed_by(child, SIGSEGV);

    child = fork_flushed();
    CHECK(child >= 0);
    if (child == 0)
    {
        struct sigaction ignore = {0};
        ignore.sa_handler = SIG_IGN;
        CHECK(sigaction(SIGSEGV, &ignore, NULL) == 0);
        fault_write();
        _exit(1); // not reached
    }
    child_killed_by(child, SIGSEGV);
}

// A handler needs a stack: without one, the process ends
static void test_no_stack_for_handler(void)
{
    pid_t child = fork_flushed();
    CHECK(child >= 0);
    if (child == 0)
    {
        handle_plain(SIGSEGV, on_fault_exit);
        __asm__ __volatile__("movq $16, %%rsp\n\tmovq $0, (%%rsp)" : : : "memory");
        __builtin_unreachable();
    }
    child_killed_by(child, SIGSEGV);
}

__attribute__((noreturn)) static void sigreturn_from(void *stack)
{
    __asm__ __volatile__("movq %0, %%rsp\n\tint $0x80" : : "r"(stack), "a"(SYS_SIGRETURN) : "memory");
    __builtin_unreachable();
}

// sigreturn trusts nothing: the frame is user memory, and a program can call it with no signal at all
static void test_forged_frame(void)
{
    static const uint64_t words[] = {NOT_CANONICAL, NOT_USER_ADDRESS};
    static uint64_t junk[JUNK_WORDS];

    for (size_t i = 0; i < sizeof(words) / sizeof(words[0]); i++)
    {
        for (size_t word = 0; word < JUNK_WORDS; word++)
            junk[word] = words[i];

        pid_t child = fork_flushed();
        CHECK(child >= 0);
        if (child == 0)
            sigreturn_from(&junk[JUNK_WORDS / 2]);
        child_killed_by(child, SIGSEGV);
    }

    pid_t child = fork_flushed();
    CHECK(child >= 0);
    if (child == 0)
        sigreturn_from((void *)16);
    child_killed_by(child, SIGSEGV);
}

int main(int argc, char **argv)
{
    if (argc > 1 && strcmp(argv[1], "after-exec") == 0)
        return after_exec();
    if (argc > 1 && strcmp(argv[1], "after-spawn") == 0)
        return after_spawn();

    isolated(test_handler_runs);
    isolated(test_alignment);
    isolated(test_old_action);
    isolated(test_ignore);
    isolated(test_errors);
    isolated(test_set_functions);
    isolated(test_mask);
    isolated(test_unblockable);
    isolated(test_nesting);
    isolated(test_registers);
    isolated(test_red_zone);
    isolated(test_big_handler);
    isolated(test_eintr_read);
    isolated(test_eintr_write);
    isolated(test_eintr_wait);
    isolated(test_stdio_retries);
    isolated(test_fork);
    isolated(test_exec);
    isolated(test_spawn);
    test_fault_handled();
    test_fault_not_refused();
    test_no_stack_for_handler();
    test_forged_frame();

    // Every pipe is closed again: the next free fd is 3
    int fd = dup(0);
    CHECK(fd == 3);
    close(fd);
    return test_status();
}
