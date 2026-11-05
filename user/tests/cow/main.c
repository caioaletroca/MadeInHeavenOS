#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <test.h>

/*
 * Copy-on-write: fork shares every page, read-only, and the first write to one
 * makes a private copy. Nothing about a program may change, so each case
 * checks what must still hold when pages really are shared: the big block only
 * fits because of it; writes by either side, in either order, stay private;
 * and the kernel's own writes obey the same rules, though it reaches user
 * memory without going through the page protection. Children count their own
 * failures and report them through their exit status.
 */

#define SIGSEGV 11 // no <signal.h> until C5
#define PAGE 4096
#define KERNEL_ADDRESS 0xFFFFFFFF80000000UL
#define BLOCK_PAGES 32768 // 128 MiB
#define CHILDREN 16       // alive at once: 2 GiB as full copies, and QEMU has 1 GiB
#define SAMPLE 256        // pages between the ones a child checks
#define HEAP_PAGES 8
#define ROUNDS 200
#define TEST_FILE "/usr/tests/hello"

static char shared[2 * PAGE] __attribute__((aligned(PAGE))); // .bss: writable, so shared at a fork
static volatile int value = 1;
static const char constant[PAGE] __attribute__((aligned(PAGE))) = "constant"; // read-only, for good

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

// Waits for pid and checks it exited with status 0
static void child_passed(pid_t pid)
{
    int status = -1;
    CHECK(waitpid(pid, &status, 0) == pid);
    CHECK(WIFEXITED(status));
    CHECK(WEXITSTATUS(status) == 0);
}

static int all_bytes(const char *data, size_t size, char byte)
{
    for (size_t i = 0; i < size; i++)
        if (data[i] != byte)
            return 0;
    return 1;
}

// More memory than exists, if each child had its own. A failed fork here means pages were copied
static void test_many_children(void)
{
    size_t size = (size_t)BLOCK_PAGES * PAGE;
    char *block = sbrk((intptr_t)size);
    CHECK(block != (char *)-1);
    if (block == (char *)-1)
        return;

    for (size_t page = 0; page < BLOCK_PAGES; page++)
        *(size_t *)(block + page * PAGE) = page + 1;

    // Each child stays alive until the write end's EOF, so all of them exist together
    int hold[2];
    CHECK(pipe(hold) == 0);

    pid_t pids[CHILDREN];
    int started = 0;
    for (int i = 0; i < CHILDREN; i++)
    {
        pid_t pid = fork_flushed();
        CHECK(pid >= 0);
        if (pid < 0)
            break;

        if (pid == 0)
        {
            close(hold[1]);

            // One write: this child's own page, a copy only of that one
            *(size_t *)(block + (size_t)i * PAGE) = 9000 + (size_t)i;

            char byte;
            CHECK(read(hold[0], &byte, 1) == 0);

            for (size_t page = CHILDREN; page < BLOCK_PAGES; page += SAMPLE)
                CHECK(*(size_t *)(block + page * PAGE) == page + 1);
            CHECK(*(size_t *)(block + (size_t)i * PAGE) == 9000 + (size_t)i);
            child_done();
        }
        pids[started++] = pid;
    }

    close(hold[0]);
    close(hold[1]);
    for (int i = 0; i < started; i++)
        child_passed(pids[i]);

    // None of their writes reached the parent
    for (size_t page = 0; page < CHILDREN; page++)
        CHECK(*(size_t *)(block + page * PAGE) == page + 1);

    CHECK(sbrk(-(intptr_t)size) != (void *)-1);
}

// The parent writes first, to a page it just used: without a TLB flush at the fork, that write
// would still see the old permission and land in the page the child is sharing
static void test_parent_writes_first(void)
{
    int order[2];
    CHECK(pipe(order) == 0);

    value = 1;
    pid_t child = fork_flushed();
    CHECK(child >= 0);
    if (child == 0)
    {
        close(order[1]);
        char byte;
        CHECK(read(order[0], &byte, 1) == 0);
        CHECK(value == 1);
        value = 3;
        CHECK(value == 3);
        child_done();
    }

    close(order[0]);
    value = 2;
    close(order[1]);
    child_passed(child);
    CHECK(value == 2);
}

// The other way round, then back: the parent's next write finds a page only it uses
static void test_child_writes_first(void)
{
    value = 1;
    pid_t child = fork_flushed();
    CHECK(child >= 0);
    if (child == 0)
    {
        value = 2;
        CHECK(value == 2);
        child_done();
    }

    child_passed(child);
    CHECK(value == 1);
    value = 4;
    CHECK(value == 4);
}

// Three sharers of one page, each ending with its own value
static void test_generations(void)
{
    value = 10;
    pid_t child = fork_flushed();
    CHECK(child >= 0);
    if (child == 0)
    {
        pid_t grandchild = fork_flushed();
        CHECK(grandchild >= 0);
        if (grandchild == 0)
        {
            CHECK(value == 10);
            value = 30;
            CHECK(value == 30);
            child_done();
        }

        value = 20;
        child_passed(grandchild);
        CHECK(value == 20);
        child_done();
    }

    child_passed(child);
    CHECK(value == 10);
}

/*
 * The kernel writes through its own mapping of physical memory, where the page
 * tables' read-only bit does not apply. Each of these goes through a page the
 * parent is sharing, one of them across two pages. Written straight into the
 * shared frame, they would show up in the parent.
 */
static void test_kernel_writes(void)
{
    memset(shared, 'a', sizeof(shared));

    pid_t child = fork_flushed();
    CHECK(child >= 0);
    if (child == 0)
    {
        int fd = open(TEST_FILE, O_RDONLY);
        CHECK(fd >= 0);
        CHECK(read(fd, shared + PAGE - 2, 4) == 4);
        CHECK(memcmp(shared + PAGE - 2, "\177ELF", 4) == 0);
        close(fd);

        CHECK(stat(TEST_FILE, (struct stat *)shared) == 0);
        CHECK(getcwd(shared + 256, 64) != NULL);

        int *ends = (int *)(shared + 512);
        CHECK(pipe(ends) == 0);
        close(ends[0]);
        close(ends[1]);

        pid_t grandchild = fork_flushed();
        if (grandchild == 0)
            _exit(7);
        int *status = (int *)(shared + PAGE + 16);
        CHECK(waitpid(grandchild, status, 0) == grandchild);
        CHECK(WEXITSTATUS(*status) == 7);
        child_done();
    }

    child_passed(child);
    CHECK(all_bytes(shared, sizeof(shared), 'a'));

    // The parent is the last user now: the same writes reach its own memory
    int fd = open(TEST_FILE, O_RDONLY);
    CHECK(fd >= 0);
    CHECK(read(fd, shared, 4) == 4);
    CHECK(memcmp(shared, "\177ELF", 4) == 0);
    close(fd);
}

// A page that was never writable stays that way for the kernel too: both are shared after a fork
static void test_read_only(void)
{
    int fd = open(TEST_FILE, O_RDONLY);
    CHECK(fd >= 0);

    const unsigned char *code = (const unsigned char *)test_read_only;
    unsigned char first = code[0];

    errno = 0;
    CHECK(read(fd, (void *)constant, 4) == -1 && errno == EFAULT);
    CHECK(strcmp(constant, "constant") == 0);

    pid_t child = fork_flushed();
    CHECK(child >= 0);
    if (child == 0)
    {
        errno = 0;
        CHECK(read(fd, (void *)constant, 4) == -1 && errno == EFAULT);
        errno = 0;
        CHECK(read(fd, (void *)code, 1) == -1 && errno == EFAULT);
        child_done();
    }

    child_passed(child);
    CHECK(strcmp(constant, "constant") == 0);
    CHECK(code[0] == first);
    close(fd);
}

// A write fault outside user memory is no copy-on-write: retrying it would fault forever
static void test_kernel_address(void)
{
    pid_t child = fork_flushed();
    CHECK(child >= 0);
    if (child == 0)
    {
        *(volatile int *)KERNEL_ADDRESS = 1;
        _exit(0); // not reached
    }

    int status = 0;
    CHECK(waitpid(child, &status, 0) == child);
    CHECK(WIFSIGNALED(status) && WTERMSIG(status) == SIGSEGV);
}

// The break moved by one side: a shared page unmapped there stays in the other
static void test_break(void)
{
    size_t size = HEAP_PAGES * PAGE;
    char *heap = sbrk((intptr_t)size);
    CHECK(heap != (char *)-1);
    memset(heap, 'h', size);

    pid_t child = fork_flushed();
    CHECK(child >= 0);
    if (child == 0)
    {
        CHECK(sbrk(-(intptr_t)size) != (void *)-1);
        CHECK(sbrk((intptr_t)size) == heap);
        CHECK(all_bytes(heap, size, 0)); // new pages, not the old ones back
        child_done();
    }

    child_passed(child);
    CHECK(all_bytes(heap, size, 'h'));
    CHECK(sbrk(-(intptr_t)size) != (void *)-1);
}

// An exec gives its shares back; each round the parent is left with its page and keeps writing
static void test_rounds(void)
{
    char *argv[] = {TEST_FILE, NULL};

    for (int i = 0; i < ROUNDS; i++)
    {
        value = i;
        pid_t child = fork_flushed();
        CHECK(child >= 0);
        if (child == 0)
        {
            if (i % 2 == 0)
                _exit(42);
            execve(TEST_FILE, argv, NULL);
            _exit(99);
        }

        value = i + 1;
        int status = 0;
        CHECK(waitpid(child, &status, 0) == child);
        CHECK(WIFEXITED(status) && WEXITSTATUS(status) == 42);
        CHECK(value == i + 1);
    }
}

int main(void)
{
    test_many_children();
    test_parent_writes_first();
    test_child_writes_first();
    test_generations();
    test_kernel_writes();
    test_read_only();
    test_kernel_address();
    test_break();
    test_rounds();

    // Every pipe and file is closed again: the next free fd is 3
    int fd = dup(0);
    CHECK(fd == 3);
    close(fd);
    return test_status();
}
