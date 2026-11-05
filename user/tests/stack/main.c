#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <test.h>

/*
 * Stack growth: the stack is mapped a page at a time, on first touch, up to
 * 8 MiB below the top (USER_STACK_TOP and USER_STACK_MAX in the kernel's
 * <asm/memory.h>). Programs may go deep, jump far with one big local, read a
 * page before writing it, and hand the kernel memory they never touched. Below
 * the limit is a fault like any other. Children count their own failures and
 * report them through their exit status.
 */

#define SIGSEGV 11 // no <signal.h> until C5
#define PAGE 4096
#define STACK_TOP 0x00007FFFFFFFF000UL
#define STACK_MAX (8UL * 1024 * 1024)
#define FRAME 1024                   // bytes of local data in each recursive call
#define DEPTH 6000                   // 6 MiB of them
#define FAR (6UL * 1024 * 1024)      // a local too big to touch page by page
#define GROWTH (4UL * 1024 * 1024)   // what a child of test_rounds uses
#define ROUNDS 256                   // 1 GiB in total: leaked pages would run the machine out
#define TEST_FILE "/usr/tests/hello"

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
    CHECK(WIFEXITED(status));
    CHECK(WEXITSTATUS(status) == 0);
}

static void child_segfaulted(pid_t pid)
{
    int status = 0;
    CHECK(waitpid(pid, &status, 0) == pid);
    CHECK(WIFSIGNALED(status) && WTERMSIG(status) == SIGSEGV);
}

// Every call's frame holds its own data, checked on the way back up
__attribute__((noinline)) static long descend(int depth)
{
    volatile char data[FRAME];
    for (size_t i = 0; i < FRAME; i += 64)
        data[i] = (char)(depth + (int)i);

    long sum = depth == 0 ? 0 : descend(depth - 1);

    for (size_t i = 0; i < FRAME; i += 64)
        CHECK(data[i] == (char)(depth + (int)i));
    return sum + 1;
}

static void test_deep(void)
{
    CHECK(descend(DEPTH) == DEPTH + 1);

    // The pages stay: a second trip finds them, with the old frames' bytes gone over
    CHECK(descend(DEPTH) == DEPTH + 1);
}

// One big local: the first touch lands far below anything mapped, with a gap of untouched pages
__attribute__((noinline)) static void test_jump(void)
{
    volatile char far[FAR];

    far[0] = 'a';
    far[FAR / 2] = 'b';
    far[FAR - 1] = 'c';
    CHECK(far[0] == 'a' && far[FAR / 2] == 'b' && far[FAR - 1] == 'c');
}

// A read can be the first touch too, and a new page never shows old data
__attribute__((noinline)) static void test_read_first(void)
{
    volatile char far[FAR];

    for (size_t i = 0; i < FAR; i += 257 * PAGE)
        CHECK(far[i] == 0);
}

// The kernel touches these pages first, in both directions
__attribute__((noinline)) static void test_kernel_first(void)
{
    char far[FAR];

    // Into a page, across two, and a struct at the far end
    int fd = open(TEST_FILE, O_RDONLY);
    CHECK(fd >= 0);
    CHECK(read(fd, far, 4) == 4);
    CHECK(memcmp(far, "\177ELF", 4) == 0);
    CHECK(read(fd, far + 3 * PAGE - 2, 4) == 4);
    close(fd);

    struct stat *st = (struct stat *)(far + 5 * PAGE);
    CHECK(stat(TEST_FILE, st) == 0 && st->st_size > 0);
    CHECK(getcwd(far + 7 * PAGE, 64) != NULL);

    // Out of one: zeros, not EFAULT
    int fds[2];
    CHECK(pipe(fds) == 0);
    CHECK(write(fds[1], far + 2 * 1024 * 1024, 16) == 16);
    char bytes[16];
    CHECK(read(fds[0], bytes, sizeof(bytes)) == 16);
    for (size_t i = 0; i < sizeof(bytes); i++)
        CHECK(bytes[i] == 0);
    close(fds[0]);
    close(fds[1]);
}

// The last page of the limit works; the one below it is outside the stack
static void test_limit(void)
{
    volatile char *lowest = (volatile char *)(STACK_TOP - STACK_MAX);

    pid_t child = fork_flushed();
    CHECK(child >= 0);
    if (child == 0)
    {
        *lowest = 'x';
        CHECK(*lowest == 'x');
        child_done();
    }
    child_passed(child);

    child = fork_flushed();
    CHECK(child >= 0);
    if (child == 0)
    {
        lowest[-1] = 'x';
        _exit(0); // not reached
    }
    child_segfaulted(child);

    // To the kernel, too
    errno = 0;
    CHECK(write(1, (const void *)(lowest - PAGE), 4) == -1 && errno == EFAULT);
}

__attribute__((noinline)) static long overflow(long depth)
{
    volatile char data[FRAME];
    data[0] = (char)depth;
    return depth < 0 ? 0 : overflow(depth + 1) + data[0];
}

// Recursion with no end: a SIGSEGV, not the machine's memory
static void test_overflow(void)
{
    pid_t child = fork_flushed();
    CHECK(child >= 0);
    if (child == 0)
    {
        overflow(0);
        _exit(0); // not reached
    }
    child_segfaulted(child);
}

__attribute__((noinline)) static void fill_stack(size_t size, char byte)
{
    volatile char area[size];
    for (size_t i = 0; i < size; i += PAGE)
        area[i] = byte;
    for (size_t i = 0; i < size; i += PAGE)
        CHECK(area[i] == byte);
}

// Grown pages are copy-on-write like any others, and each side grows its own
__attribute__((noinline)) static void test_fork(void)
{
    volatile char mine[2 * 1024 * 1024];
    for (size_t i = 0; i < sizeof(mine); i += PAGE)
        mine[i] = 'p';

    pid_t child = fork_flushed();
    CHECK(child >= 0);
    if (child == 0)
    {
        for (size_t i = 0; i < sizeof(mine); i += PAGE)
        {
            CHECK(mine[i] == 'p');
            mine[i] = 'c';
        }
        fill_stack(FAR - sizeof(mine), 'c');
        child_done();
    }

    child_passed(child);
    for (size_t i = 0; i < sizeof(mine); i += PAGE)
        CHECK(mine[i] == 'p');
}

// A new program starts with a new stack: the old one's bytes are gone
__attribute__((noinline)) static int fresh_stack(void)
{
    volatile char far[GROWTH];

    // Not its last pages: libc's startup has run on those
    for (size_t i = 0; i < GROWTH - 16 * PAGE; i += PAGE)
        CHECK(far[i] == 0);
    return test_status();
}

static void test_exec(char *self)
{
    pid_t child = fork_flushed();
    CHECK(child >= 0);
    if (child == 0)
    {
        fill_stack(GROWTH, 'x');
        char *argv[] = {self, "fresh", NULL};
        execve(self, argv, NULL);
        _exit(99);
    }
    child_passed(child);
}

// Every grown page comes back with its process
static void test_rounds(void)
{
    for (int i = 0; i < ROUNDS; i++)
    {
        pid_t child = fork_flushed();
        CHECK(child >= 0);
        if (child == 0)
        {
            fill_stack(GROWTH, 'r');
            child_done();
        }
        child_passed(child);
    }
}

int main(int argc, char **argv)
{
    if (argc > 1)
        return fresh_stack();

    // First, while these pages are untouched: tests below leave theirs mapped
    test_read_first();
    test_kernel_first();

    test_deep();
    test_jump();
    test_limit();
    test_overflow();
    test_fork();
    test_exec(argv[0]);
    test_rounds();

    // Every pipe and file is closed again: the next free fd is 3
    int fd = dup(0);
    CHECK(fd == 3);
    close(fd);
    return test_status();
}
