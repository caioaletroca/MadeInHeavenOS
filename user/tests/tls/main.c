#include <sys/wait.h>
#include <mihos/syscall.h>
#include <errno.h>
#include <signal.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <test.h>

/*
 * The thread pointer (TLS): each thread's own FS base, set by a system call.
 * Code reaches its thread's block through %fs-relative loads, as compiled
 * __thread variables do. The kernel loads it when the thread switches in,
 * keeps it at fork (the child's memory is a copy) and clears it at exec (the
 * old block is gone). Our libc has no TLS and no wrapper: the call is made
 * raw, as mlibc's sysdeps will make it.
 */

#define SELF "/usr/tests/tls"
#define AFTER_EXEC "after-exec"
#define SWITCH_ROUNDS 5
#define SLEEP_MS 20

// The x86-64 ABI's layout: the first word points to the block itself
typedef struct tcb
{
    struct tcb *self;
    long value;
} tcb_t;

static long set_thread_pointer(void *pointer)
{
    long result;
    __asm__ __volatile__("int $0x80"
                         : "=a"(result)
                         : "a"((long)SYS_SET_THREAD_POINTER), "D"(pointer)
                         : "rcx", "r11", "memory");
    return result;
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

static void sleep_ms(long ms)
{
    struct timespec t = {0, ms * 1000000L};
    nanosleep(&t, NULL);
}

static int wait_child(pid_t pid)
{
    int status = -1;
    CHECK(pid > 0 && waitpid(pid, &status, 0) == pid);
    return status;
}

static tcb_t parent_block = {&parent_block, 111};

static void test_set_and_read(void)
{
    CHECK(set_thread_pointer(&parent_block) == 0);
    CHECK(fs_self() == &parent_block);
    CHECK(fs_value() == 111);

    // Writes through the block show through FS: it is the same memory
    parent_block.value = 112;
    CHECK(fs_value() == 112);
    parent_block.value = 111;
}

// Each side keeps its own pointer across many switches; the child starts with the parent's
static void test_switches(void)
{
    static tcb_t child_block;

    fflush(stdout);
    pid_t pid = fork();
    if (pid == 0)
    {
        test_failures = 0;
        int ok = fs_self() == &parent_block && fs_value() == 111; // inherited at fork

        child_block = (tcb_t){&child_block, 222};
        ok = ok && set_thread_pointer(&child_block) == 0;
        for (int round = 0; round < SWITCH_ROUNDS; round++)
        {
            sleep_ms(SLEEP_MS);
            ok = ok && fs_self() == &child_block && fs_value() == 222;
        }
        _exit(ok ? 0 : 1);
    }

    for (int round = 0; round < SWITCH_ROUNDS; round++)
    {
        sleep_ms(SLEEP_MS);
        CHECK(fs_self() == &parent_block && fs_value() == 111);
    }

    int status = wait_child(pid);
    CHECK(WIFEXITED(status) && WEXITSTATUS(status) == 0);
    CHECK(fs_self() == &parent_block); // the child's change stayed in the child
}

static volatile long seen_in_handler;

static void on_signal(int signal)
{
    (void)signal;
    seen_in_handler = fs_value();
}

// A handler runs on the same thread: the same block
static void test_signal_handler(void)
{
    struct sigaction action = {0};
    action.sa_handler = on_signal;
    sigemptyset(&action.sa_mask);
    CHECK(sigaction(SIGUSR1, &action, NULL) == 0);

    seen_in_handler = 0;
    CHECK(kill(getpid(), SIGUSR1) == 0);
    CHECK(seen_in_handler == 111);
}

// The new program starts with no thread pointer: %fs:0 reads address 0
static void test_exec_clears(void)
{
    fflush(stdout);
    pid_t pid = fork();
    if (pid == 0)
    {
        char *argv[] = {SELF, AFTER_EXEC, NULL};
        execv(SELF, argv);
        _exit(100); // exec failed
    }

    int status = wait_child(pid);
    CHECK(WIFSIGNALED(status) && WTERMSIG(status) == SIGSEGV);
}

// Kernel and non-canonical addresses are refused (wrmsr would fault in the kernel); 0 clears
static void test_errors(void)
{
    CHECK(set_thread_pointer((void *)0x0000800000000000UL) == -EINVAL); // USER_TOP
    CHECK(set_thread_pointer((void *)0x8000000000000000UL) == -EINVAL); // not canonical
    CHECK(set_thread_pointer((void *)0xFFFFFFFF80000000UL) == -EINVAL); // the kernel
    CHECK(fs_self() == &parent_block);                                  // unchanged

    fflush(stdout);
    pid_t pid = fork();
    if (pid == 0)
    {
        if (set_thread_pointer(NULL) != 0)
            _exit(1);
        fs_self(); // address 0
        _exit(0);
    }
    int status = wait_child(pid);
    CHECK(WIFSIGNALED(status) && WTERMSIG(status) == SIGSEGV);
}

int main(int argc, char **argv)
{
    // Run again by test_exec_clears: this must fault
    if (argc > 1 && strcmp(argv[1], AFTER_EXEC) == 0)
    {
        fs_self();
        return 0;
    }

    test_set_and_read();
    test_switches();
    test_signal_handler();
    test_exec_clears();
    test_errors();
    return test_status();
}
