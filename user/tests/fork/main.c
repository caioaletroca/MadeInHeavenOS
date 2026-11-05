#include <sys/wait.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <test.h>

/*
 * fork: one call, two returns. The child is a copy of the caller: its own
 * memory (globals, stack, heap, the break), its own fd table and directory,
 * the same open files. Each case forks a child that checks its side and
 * leaves with child_done, so the parent's wait sees the child's failures in
 * its status. stdout is flushed before every fork: a child would otherwise
 * inherit the unprinted bytes and print them too, as on any POSIX system.
 */

#define SIGSEGV 11       // no <signal.h> until C5
#define HEAP_PAGES 8     // a block over several pages: every page must be copied
#define PAGE 4096
#define DEPTH 50         // calls fork returns through, in both processes
#define CHILDREN 4       // alive at once
#define ROUNDS 200       // forks in a row: each one is a whole copy, freed again
#define ORPHAN_SPINS 100000000L
#define TEST_FILE "/usr/tests/hello"

static int global = 10;
static int zeroed; // in .bss
static const int read_only = 7;

static pid_t fork_flushed(void)
{
    fflush(stdout);
    return fork();
}

// Leaves the child: exit status = its failed checks. _exit, so atexit never runs twice
static void child_done(void)
{
    fflush(stdout);
    _exit(test_status());
}

// Waits for pid and checks it exited with status 0
static void child_passed(pid_t pid)
{
    int status = -1;
    CHECK(waitpid(pid, &status, 0) == pid);
    CHECK(WIFEXITED(status));
    CHECK(WEXITSTATUS(status) == 0);
}

// The byte at position i of the heap block
static unsigned char pattern(size_t i)
{
    return (unsigned char)(i * 7 % 251);
}

static void test_returns(void)
{
    pid_t parent = getpid();
    pid_t child = fork_flushed();
    CHECK(child >= 0);
    if (child == 0)
    {
        CHECK(getpid() != parent);
        CHECK(getppid() == parent);
        child_done();
    }

    CHECK(child != parent);
    CHECK(getpid() == parent);
    child_passed(child);

    // The child's exit status comes back, as with spawn
    child = fork_flushed();
    if (child == 0)
        _exit(42);
    int status = 0;
    CHECK(waitpid(child, &status, 0) == child);
    CHECK(WIFEXITED(status) && WEXITSTATUS(status) == 42);

    // Already waited for
    CHECK(waitpid(child, &status, 0) == -1 && errno == ECHILD);
}

// fork deep in the stack: both processes return through every frame, each with its own copy
static int return_through(int depth)
{
    int here = depth * 3;
    int result;
    if (depth == 0)
        result = fork_flushed() == 0 ? 1000 : 2000;
    else
        result = return_through(depth - 1);
    CHECK(here == depth * 3);
    return result + 1;
}

static void test_stack(void)
{
    int result = return_through(DEPTH);
    if (result == 1000 + DEPTH + 1)
        child_done();

    CHECK(result == 2000 + DEPTH + 1);
    int status = -1;
    CHECK(wait(&status) > 0);
    CHECK(WIFEXITED(status) && WEXITSTATUS(status) == 0);
}

// Each side changes every kind of memory; the other keeps its values. A pipe orders them
static void test_memory(void)
{
    int order[2];
    CHECK(pipe(order) == 0);

    global = 10;
    zeroed = 0;
    int local = 20;
    size_t size = HEAP_PAGES * PAGE;
    unsigned char *heap = malloc(size);
    CHECK(heap != NULL);
    for (size_t i = 0; i < size; i++)
        heap[i] = pattern(i);

    pid_t child = fork_flushed();
    CHECK(child >= 0);
    if (child == 0)
    {
        close(order[1]);
        // Everything was copied, on every page of the block
        for (size_t i = 0; i < size; i++)
            if (heap[i] != pattern(i))
            {
                CHECK(heap[i] == pattern(i));
                break;
            }

        // Sees none of the parent's writes: they happen before the parent's close, read's EOF
        char byte;
        CHECK(read(order[0], &byte, 1) == 0);
        CHECK(global == 10 && zeroed == 0 && local == 20);
        CHECK(heap[0] == pattern(0) && heap[size - 1] == pattern(size - 1));

        global = 111;
        zeroed = 222;
        local = 333;
        memset(heap, 0xAA, size);
        child_done();
    }

    close(order[0]);
    global = 11;
    zeroed = 12;
    local = 13;
    heap[0] = 0x55;
    heap[size - 1] = 0x66;
    close(order[1]);

    child_passed(child);
    CHECK(global == 11 && zeroed == 12 && local == 13);
    CHECK(heap[0] == 0x55 && heap[size - 1] == 0x66);
    for (size_t i = 1; i < size - 1; i++)
        if (heap[i] != pattern(i))
        {
            CHECK(heap[i] == pattern(i));
            break;
        }
    free(heap);
}

// Each process has its own break, starting from the same place
static void test_break(void)
{
    void *before = sbrk(0);
    pid_t child = fork_flushed();
    CHECK(child >= 0);
    if (child == 0)
    {
        CHECK(sbrk(0) == before);
        char *grown = sbrk(PAGE * 4);
        CHECK(grown != (char *)-1);
        grown[PAGE * 4 - 1] = 1;
        CHECK(sbrk(0) == (char *)before + PAGE * 4);
        child_done();
    }

    child_passed(child);
    CHECK(sbrk(0) == before);
}

// A page the program cannot write stays that way in the child: flags are copied too
static void test_read_only(void)
{
    pid_t child = fork_flushed();
    CHECK(child >= 0);
    if (child == 0)
    {
        *(volatile int *)&read_only = 8;
        _exit(0); // not reached
    }

    int status = 0;
    CHECK(waitpid(child, &status, 0) == child);
    CHECK(WIFSIGNALED(status) && WTERMSIG(status) == SIGSEGV);
}

// The table is copied, so closing is private; the open files are shared, with their offset
static void test_files(void)
{
    int fd = open(TEST_FILE, O_RDONLY);
    CHECK(fd >= 0);

    char bytes[4];
    pid_t child = fork_flushed();
    CHECK(child >= 0);
    if (child == 0)
    {
        CHECK(read(fd, bytes, sizeof(bytes)) == sizeof(bytes));
        CHECK(close(fd) == 0);
        CHECK(read(fd, bytes, 1) == -1 && errno == EBADF);
        child_done();
    }

    child_passed(child);
    CHECK(lseek(fd, 0, SEEK_CUR) == sizeof(bytes));
    CHECK(read(fd, bytes, 1) == 1);
    close(fd);
}

// A pipe between the two: the use fork was made for, without any fd moved around a spawn
static void test_pipe(void)
{
    int fds[2];
    CHECK(pipe(fds) == 0);

    pid_t child = fork_flushed();
    CHECK(child >= 0);
    if (child == 0)
    {
        close(fds[0]);
        CHECK(dup2(fds[1], 1) == 1);
        close(fds[1]);
        CHECK(write(1, "from the child", 14) == 14);
        _exit(test_status());
    }

    close(fds[1]);
    char text[32];
    CHECK(read(fds[0], text, sizeof(text)) == 14);
    CHECK(memcmp(text, "from the child", 14) == 0);
    CHECK(read(fds[0], text, sizeof(text)) == 0); // EOF: the child's copy went with it
    close(fds[0]);
    child_passed(child);
}

static void test_directory(void)
{
    char start[128], here[128];
    CHECK(getcwd(start, sizeof(start)) != NULL);

    pid_t child = fork_flushed();
    CHECK(child >= 0);
    if (child == 0)
    {
        CHECK(chdir("/usr/tests") == 0);
        CHECK(getcwd(here, sizeof(here)) != NULL && strcmp(here, "/usr/tests") == 0);
        child_done();
    }

    child_passed(child);
    CHECK(getcwd(here, sizeof(here)) != NULL && strcmp(here, start) == 0);
}

// Several at once, any order; each reaped once
static void test_children(void)
{
    pid_t pids[CHILDREN];
    for (int i = 0; i < CHILDREN; i++)
    {
        pids[i] = fork_flushed();
        CHECK(pids[i] >= 0);
        if (pids[i] == 0)
            _exit(i + 1);
    }

    int seen = 0;
    for (int i = 0; i < CHILDREN; i++)
    {
        int status = 0;
        pid_t pid = wait(&status);
        CHECK(pid > 0 && WIFEXITED(status));
        int number = WEXITSTATUS(status);
        CHECK(number >= 1 && number <= CHILDREN && pids[number - 1] == pid);
        seen |= 1 << number;
    }
    CHECK(seen == ((1 << (CHILDREN + 1)) - 2));
    CHECK(wait(NULL) == -1 && errno == ECHILD);
}

// A child forking too; its parent leaves first, so init adopts the grandchild
static void test_orphan(void)
{
    int fds[2];
    CHECK(pipe(fds) == 0);

    pid_t child = fork_flushed();
    CHECK(child >= 0);
    if (child == 0)
    {
        pid_t grandchild = fork_flushed();
        if (grandchild != 0)
            _exit(grandchild < 0);

        long spins = 0;
        while (getppid() != 1 && spins < ORPHAN_SPINS)
            spins++;
        pid_t adopted = getppid();
        write(fds[1], &adopted, sizeof(adopted));
        _exit(0);
    }

    child_passed(child);
    close(fds[1]);
    pid_t adopted = -1;
    CHECK(read(fds[0], &adopted, sizeof(adopted)) == sizeof(adopted));
    CHECK(adopted == 1);
    close(fds[0]);
}

// Each fork builds a whole address space and each exit frees it
static void test_rounds(void)
{
    for (int i = 0; i < ROUNDS; i++)
    {
        pid_t child = fork_flushed();
        CHECK(child >= 0);
        if (child == 0)
            _exit(i & 0x7F);

        int status = -1;
        CHECK(waitpid(child, &status, 0) == child);
        CHECK(WIFEXITED(status) && WEXITSTATUS(status) == (i & 0x7F));
    }
}

int main(void)
{
    test_returns();
    test_stack();
    test_memory();
    test_break();
    test_read_only();
    test_files();
    test_pipe();
    test_directory();
    test_children();
    test_orphan();
    test_rounds();

    // Every pipe and file is closed again: the next free fd is 3
    int fd = dup(0);
    CHECK(fd == 3);
    close(fd);
    return test_status();
}
