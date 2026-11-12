#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <test.h>

/*
 * mmap and munmap: anonymous private memory. mmap only reserves a range (a
 * region); each page is mapped, zeroed, the first time it is touched, with the
 * region's protection. Regions are placed top-down below the stack, never over
 * the heap, and brk stops below them. A fork copies them, the pages private
 * as any other. Accesses that must fail run in a child and end it with SIGSEGV.
 */

#define PAGE 4096UL
#define HUGE_SIZE (4UL << 30) // 4 GiB: more than the machine has, fine while untouched
#define STACK_TOP 0x00007FFFFFFFF000UL
#define STACK_MAX (8UL << 20)

static void *map(size_t size, int protection)
{
    return mmap(NULL, size, protection, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
}

static int all_zero(const char *data, size_t size)
{
    for (size_t i = 0; i < size; i++)
        if (data[i] != 0)
            return 0;
    return 1;
}

// Runs touch(address, write) in a child and checks that it died of SIGSEGV
static void faults(volatile char *address, int write)
{
    fflush(stdout);
    pid_t pid = fork();
    if (pid == 0)
    {
        if (write)
            *address = 1;
        else
            (void)*address;
        _exit(0); // the access worked: wrong
    }

    int status = -1;
    CHECK(pid > 0 && waitpid(pid, &status, 0) == pid);
    CHECK(WIFSIGNALED(status) && WTERMSIG(status) == SIGSEGV);
}

// Pages come zeroed, aligned, between the heap and the stack, and keep what is written
static void test_map(void)
{
    char *a = map(3 * PAGE, PROT_READ | PROT_WRITE);
    CHECK(a != MAP_FAILED);
    if (a == MAP_FAILED)
        return;

    CHECK(((uintptr_t)a & (PAGE - 1)) == 0);
    CHECK((uintptr_t)a >= (uintptr_t)sbrk(0));
    CHECK((uintptr_t)a + 3 * PAGE <= STACK_TOP - STACK_MAX);
    CHECK(all_zero(a, 3 * PAGE));

    for (size_t i = 0; i < 3; i++)
        memset(a + i * PAGE, 'a' + (int)i, PAGE);
    CHECK(a[0] == 'a' && a[PAGE] == 'b' && a[3 * PAGE - 1] == 'c');

    // Top-down: the next one goes below, a size that is not whole pages rounded up
    char *b = map(100, PROT_READ | PROT_WRITE);
    CHECK(b != MAP_FAILED && b + PAGE <= a);
    if (b != MAP_FAILED)
    {
        CHECK(all_zero(b, PAGE)); // the whole page, past the 100 bytes asked
        CHECK(munmap(b, 100) == 0);
    }

    CHECK(munmap(a, 3 * PAGE) == 0);
}

// Only touched pages cost memory: 4 GiB reserved, three pages used
static void test_touched_only(void)
{
    char *huge = map(HUGE_SIZE, PROT_READ | PROT_WRITE);
    CHECK(huge != MAP_FAILED);
    if (huge == MAP_FAILED)
        return;

    huge[0] = 'x';
    huge[HUGE_SIZE / 2] = 'y';
    huge[HUGE_SIZE - 1] = 'z';
    CHECK(huge[0] == 'x' && huge[HUGE_SIZE / 2] == 'y' && huge[HUGE_SIZE - 1] == 'z');
    CHECK(huge[PAGE] == 0);
    CHECK(munmap(huge, HUGE_SIZE) == 0);
}

// PROT_READ: reads see zeros, writes fault. PROT_NONE: every access faults
static void test_protection(void)
{
    char *read_only = map(PAGE, PROT_READ);
    char *none = map(PAGE, PROT_NONE);
    CHECK(read_only != MAP_FAILED && none != MAP_FAILED);
    if (read_only == MAP_FAILED || none == MAP_FAILED)
        return;

    faults(read_only, 1);
    CHECK(read_only[0] == 0);
    faults(read_only, 1); // mapped now, by the read: still refused
    faults(none, 0);
    faults(none, 1);

    CHECK(munmap(read_only, PAGE) == 0);
    CHECK(munmap(none, PAGE) == 0);
}

// The kernel's own copies go through the same rules: read() into a page never
// touched maps it, into a read-only one fails
static void test_system_calls(void)
{
    char *page = map(PAGE, PROT_READ | PROT_WRITE);
    char *read_only = map(PAGE, PROT_READ);
    int fds[2];
    CHECK(page != MAP_FAILED && read_only != MAP_FAILED && pipe(fds) == 0);

    CHECK(write(fds[1], "hello", 5) == 5);
    CHECK(read(fds[0], page + 10, 5) == 5);
    CHECK(memcmp(page + 10, "hello", 5) == 0);

    CHECK(write(fds[1], "hello", 5) == 5);
    errno = 0;
    CHECK(read(fds[0], read_only, 5) == -1 && errno == EFAULT);
    CHECK(read_only[0] == 0);

    close(fds[0]);
    close(fds[1]);
    munmap(page, PAGE);
    munmap(read_only, PAGE);
}

// munmap of the middle leaves both ends; an empty range is fine
static void test_unmap(void)
{
    char *a = map(3 * PAGE, PROT_READ | PROT_WRITE);
    CHECK(a != MAP_FAILED);
    if (a == MAP_FAILED)
        return;

    memset(a, 'a', 3 * PAGE);
    CHECK(munmap(a + PAGE, PAGE) == 0);
    CHECK(a[0] == 'a' && a[PAGE - 1] == 'a');
    CHECK(a[2 * PAGE] == 'a' && a[3 * PAGE - 1] == 'a');
    faults(a + PAGE, 0);

    // Unmapping it again, or across the hole, changes nothing more
    CHECK(munmap(a + PAGE, PAGE) == 0);
    CHECK(munmap(a, 3 * PAGE) == 0);
    faults(a, 0);
    faults(a + 2 * PAGE, 1);

    // The range is free again: a hint there is taken
    char *again = mmap(a, PAGE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    CHECK(again == a);
    CHECK(again != MAP_FAILED && all_zero(again, PAGE));
    munmap(again, PAGE);
}

// A hint over a region is only a hint; MAP_FIXED replaces what is there
static void test_fixed(void)
{
    char *a = map(4 * PAGE, PROT_READ | PROT_WRITE);
    CHECK(a != MAP_FAILED);
    if (a == MAP_FAILED)
        return;
    memset(a, 'a', 4 * PAGE);

    char *elsewhere = mmap(a + PAGE, PAGE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    CHECK(elsewhere != MAP_FAILED && (elsewhere + PAGE <= a || elsewhere >= a + 4 * PAGE));
    if (elsewhere != MAP_FAILED)
        munmap(elsewhere, PAGE);

    char *fixed = mmap(a + PAGE, 2 * PAGE, PROT_READ, MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);
    CHECK(fixed == a + PAGE);
    CHECK(all_zero(a + PAGE, 2 * PAGE)); // fresh pages
    CHECK(a[0] == 'a' && a[4 * PAGE - 1] == 'a');
    faults(a + PAGE, 1); // the new protection
    a[0] = 'b';          // the old one around it
    a[3 * PAGE] = 'b';

    CHECK(munmap(a, 4 * PAGE) == 0);
}

// The heap stops below a region instead of growing over it
static void test_brk(void)
{
    char *end = sbrk(0);
    char *heap_end = (char *)(((uintptr_t)end + PAGE - 1) & ~(PAGE - 1));
    char *region = mmap(heap_end + 2 * PAGE, PAGE, PROT_READ | PROT_WRITE,
                        MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);
    CHECK(region == heap_end + 2 * PAGE);
    if (region == MAP_FAILED)
        return;
    region[0] = 'r';

    CHECK(sbrk((intptr_t)PAGE) == end); // still below
    errno = 0;
    CHECK(sbrk((intptr_t)(2 * PAGE)) == (void *)-1 && errno == ENOMEM);
    CHECK(sbrk(0) == end + PAGE && region[0] == 'r');
    CHECK(sbrk(-(intptr_t)PAGE) == end + PAGE);

    // Nor does a hint put a region over the heap
    char *below = mmap(heap_end - PAGE, PAGE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    CHECK(below != MAP_FAILED && below >= heap_end);
    if (below != MAP_FAILED)
        munmap(below, PAGE);

    CHECK(munmap(region, PAGE) == 0);
}

// A child gets the regions, and its writes stay its own
static void test_fork(void)
{
    char *a = map(2 * PAGE, PROT_READ | PROT_WRITE);
    CHECK(a != MAP_FAILED);
    if (a == MAP_FAILED)
        return;
    a[0] = 'p';

    fflush(stdout);
    pid_t pid = fork();
    if (pid == 0)
    {
        int ok = a[0] == 'p' && a[PAGE] == 0; // the page nobody touched: mapped from its copy of the region
        a[0] = 'c';
        a[PAGE] = 'c';
        _exit(ok && a[0] == 'c' ? 0 : 1);
    }

    int status = -1;
    CHECK(pid > 0 && waitpid(pid, &status, 0) == pid);
    CHECK(WIFEXITED(status) && WEXITSTATUS(status) == 0);
    CHECK(a[0] == 'p' && a[PAGE] == 0);
    CHECK(munmap(a, 2 * PAGE) == 0);
}

static void expect_error(void *result, int error)
{
    CHECK(result == MAP_FAILED && errno == error);
    if (result != MAP_FAILED)
        munmap(result, PAGE);
}

static void test_errors(void)
{
    const int anonymous = MAP_PRIVATE | MAP_ANONYMOUS;
    char *a = map(PAGE, PROT_READ | PROT_WRITE);
    CHECK(a != MAP_FAILED);

    expect_error(mmap(NULL, 0, PROT_READ, anonymous, -1, 0), EINVAL);
    expect_error(mmap(NULL, PAGE, PROT_READ, MAP_ANONYMOUS, -1, 0), EINVAL); // neither shared nor private
    expect_error(mmap(NULL, PAGE, PROT_READ, anonymous | MAP_SHARED, -1, 0), EINVAL);
    expect_error(mmap(NULL, PAGE, PROT_READ, anonymous | 0x1000, -1, 0), EINVAL);
    expect_error(mmap(NULL, PAGE, 8, anonymous, -1, 0), EINVAL);
    expect_error(mmap(a + 1, PAGE, PROT_READ, anonymous | MAP_FIXED, -1, 0), EINVAL);

    // Not there yet: files, and memory shared across fork
    expect_error(mmap(NULL, PAGE, PROT_READ, MAP_PRIVATE, 0, 0), ENODEV);
    expect_error(mmap(NULL, PAGE, PROT_READ, MAP_SHARED | MAP_ANONYMOUS, -1, 0), ENODEV);

    // No room: more than the user half, or MAP_FIXED on the stack or the kernel
    expect_error(map(1UL << 47, PROT_READ), ENOMEM);
    expect_error(mmap((void *)(STACK_TOP - PAGE), PAGE, PROT_READ, anonymous | MAP_FIXED, -1, 0), ENOMEM);
    expect_error(mmap((void *)0xFFFFFFFF80000000UL, PAGE, PROT_READ, anonymous | MAP_FIXED, -1, 0), ENOMEM);

    errno = 0;
    CHECK(munmap(a + 1, PAGE) == -1 && errno == EINVAL);
    errno = 0;
    CHECK(munmap(a, 0) == -1 && errno == EINVAL);
    errno = 0;
    CHECK(munmap((void *)0xFFFFFFFF80000000UL, PAGE) == -1 && errno == EINVAL);

    // munmap never touches memory outside regions: the stack's page is still there
    volatile char on_stack = 's';
    CHECK(munmap((void *)((uintptr_t)&on_stack & ~(PAGE - 1)), PAGE) == 0);
    CHECK(on_stack == 's');

    if (a != MAP_FAILED)
        CHECK(munmap(a, PAGE) == 0);
}

int main(void)
{
    test_map();
    test_touched_only();
    test_protection();
    test_system_calls();
    test_unmap();
    test_fixed();
    test_brk();
    test_fork();
    test_errors();
    return test_status();
}
