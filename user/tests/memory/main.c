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
 * as any other. mprotect changes a range's protection, splitting regions,
 * and keeps the pages' contents. With NX, only pages asked to be executable
 * run: the program's text and PROT_EXEC mappings, never data, stack or heap.
 * Accesses that must fail run in a child and end it with SIGSEGV.
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

// ---- mprotect ----

static int all_bytes(const char *data, size_t size, char byte)
{
    for (size_t i = 0; i < size; i++)
        if (data[i] != byte)
            return 0;
    return 1;
}

// Pages keep their contents through every change, PROT_NONE included
static void test_protect_keeps_data(void)
{
    char *a = map(2 * PAGE, PROT_READ | PROT_WRITE);
    CHECK(a != MAP_FAILED);
    if (a == MAP_FAILED)
        return;
    memset(a, 'k', PAGE); // the second page stays untouched: it gets the protection when touched

    CHECK(mprotect(a, 2 * PAGE, PROT_READ) == 0);
    CHECK(all_bytes(a, PAGE, 'k') && all_zero(a + PAGE, PAGE));
    faults(a, 1);
    faults(a + PAGE, 1);

    CHECK(mprotect(a, 2 * PAGE, PROT_NONE) == 0);
    faults(a, 0);
    faults(a + PAGE, 0);

    CHECK(mprotect(a, 2 * PAGE, PROT_READ | PROT_WRITE) == 0);
    CHECK(all_bytes(a, PAGE, 'k'));
    a[0] = 'w';
    a[PAGE] = 'w';
    CHECK(a[0] == 'w' && a[PAGE] == 'w');

    CHECK(munmap(a, 2 * PAGE) == 0);
}

// The kernel's own copies obey the protection: no write into a read-only page,
// nothing read out of a PROT_NONE one (the kernel reaches it through the direct map)
static void test_protect_system_calls(void)
{
    char *a = map(PAGE, PROT_READ | PROT_WRITE);
    int fds[2];
    CHECK(a != MAP_FAILED && pipe(fds) == 0);
    memset(a, 's', PAGE);

    CHECK(mprotect(a, PAGE, PROT_READ) == 0);
    CHECK(write(fds[1], "hello", 5) == 5);
    errno = 0;
    CHECK(read(fds[0], a, 5) == -1 && errno == EFAULT);
    CHECK(write(fds[1], a, 5) == 5); // reading it is fine

    CHECK(mprotect(a, PAGE, PROT_NONE) == 0);
    errno = 0;
    CHECK(write(fds[1], a, 5) == -1 && errno == EFAULT);

    // The five 's' written while readable, and nothing from the PROT_NONE write. (The failed
    // read already took "hello" out of the pipe: sys_read reads before it copies, see the debt)
    char got[10];
    CHECK(read(fds[0], got, sizeof(got)) == 5);
    CHECK(memcmp(got, "sssss", 5) == 0);

    close(fds[0]);
    close(fds[1]);
    munmap(a, PAGE);
}

// The middle page of three: the region splits in three, both ends keep their protection
static void test_protect_middle(void)
{
    char *a = map(3 * PAGE, PROT_READ | PROT_WRITE);
    CHECK(a != MAP_FAILED);
    if (a == MAP_FAILED)
        return;
    memset(a, 'm', 3 * PAGE);

    CHECK(mprotect(a + PAGE, PAGE, PROT_READ) == 0);
    a[0] = 'x';
    a[3 * PAGE - 1] = 'x';
    faults(a + PAGE, 1);
    CHECK(a[PAGE] == 'm');

    // Back, across all three pieces at once, then a piece unmapped on its own
    CHECK(mprotect(a, 3 * PAGE, PROT_READ | PROT_WRITE) == 0);
    a[PAGE] = 'x';
    CHECK(munmap(a + 2 * PAGE, PAGE) == 0);
    CHECK(a[0] == 'x' && a[PAGE] == 'x');
    faults(a + 2 * PAGE, 0);

    CHECK(munmap(a, 2 * PAGE) == 0);
}

// The thread-stack pattern: a guard page at the bottom of a writable block
static void test_guard_page(void)
{
    char *stack = map(4 * PAGE, PROT_READ | PROT_WRITE);
    CHECK(stack != MAP_FAILED);
    if (stack == MAP_FAILED)
        return;

    CHECK(mprotect(stack, PAGE, PROT_NONE) == 0);
    memset(stack + PAGE, 's', 3 * PAGE);
    faults(stack + PAGE - 1, 1); // one byte past the bottom
    CHECK(munmap(stack, 4 * PAGE) == 0);
}

// After a fork the pages are shared: made writable again, a write copies them
static void test_protect_after_fork(void)
{
    char *a = map(PAGE, PROT_READ | PROT_WRITE);
    CHECK(a != MAP_FAILED);
    if (a == MAP_FAILED)
        return;
    a[0] = 'p';
    CHECK(mprotect(a, PAGE, PROT_READ) == 0);

    int ready[2], done[2];
    CHECK(pipe(ready) == 0 && pipe(done) == 0);
    fflush(stdout);
    pid_t pid = fork();
    if (pid == 0)
    {
        char byte;
        close(ready[1]);
        close(done[0]);
        int ok = read(ready[0], &byte, 1) == 1 && a[0] == 'p'; // after the parent's write
        CHECK(mprotect(a, PAGE, PROT_READ | PROT_WRITE) == 0);
        a[0] = 'c';
        ok = ok && a[0] == 'c';
        CHECK(write(done[1], "x", 1) == 1);
        _exit(ok ? 0 : 1);
    }
    close(ready[0]);
    close(done[1]);

    CHECK(mprotect(a, PAGE, PROT_READ | PROT_WRITE) == 0);
    a[0] = 'P'; // the frame is shared: this write gets a copy
    CHECK(write(ready[1], "x", 1) == 1);
    char byte;
    CHECK(read(done[0], &byte, 1) == 1);
    CHECK(a[0] == 'P'); // the child's write stayed in the child

    int status = -1;
    CHECK(waitpid(pid, &status, 0) == pid);
    CHECK(WIFEXITED(status) && WEXITSTATUS(status) == 0);
    close(ready[1]);
    close(done[0]);
    munmap(a, PAGE);
}

static void test_protect_errors(void)
{
    char *a = map(2 * PAGE, PROT_READ | PROT_WRITE);
    CHECK(a != MAP_FAILED);
    if (a == MAP_FAILED)
        return;
    a[0] = 'e';
    CHECK(munmap(a + PAGE, PAGE) == 0); // a gap right after the first page

    errno = 0;
    CHECK(mprotect(a + 1, PAGE, PROT_READ) == -1 && errno == EINVAL);
    errno = 0;
    CHECK(mprotect(a, PAGE, 8) == -1 && errno == EINVAL);
    CHECK(mprotect(a, 0, PROT_READ) == 0);

    // The range runs into the gap: ENOMEM, and the first page did not change
    errno = 0;
    CHECK(mprotect(a, 2 * PAGE, PROT_READ) == -1 && errno == ENOMEM);
    a[0] = 'f';
    CHECK(a[0] == 'f');

    // Memory outside regions: the stack is not one
    volatile char on_stack = 's';
    errno = 0;
    CHECK(mprotect((void *)((uintptr_t)&on_stack & ~(PAGE - 1)), PAGE, PROT_READ) == -1 && errno == ENOMEM);
    on_stack = 't';
    CHECK(on_stack == 't');

    CHECK(munmap(a, PAGE) == 0);
}

// ---- No-execute ----

// mov eax, 42; ret
#define CODE_SIZE 6
#define CODE_RESULT 42
static unsigned char data_code[CODE_SIZE] = {0xB8, 0x2A, 0x00, 0x00, 0x00, 0xC3};             // .data
static const unsigned char rodata_code[CODE_SIZE] = {0xB8, 0x2A, 0x00, 0x00, 0x00, 0xC3};     // .rodata

// Whether the CPU has NX (cpuid works in ring 3): without it every page runs, and nothing can fault
static int cpu_has_nx(void)
{
    uint32_t eax = 0x80000001, ebx, ecx = 0, edx;
    __asm__ __volatile__("cpuid" : "+a"(eax), "=b"(ebx), "+c"(ecx), "=d"(edx));
    return (edx & (1u << 20)) != 0;
}

static int call(const void *code)
{
    return ((int (*)(void))(uintptr_t)code)();
}

// Calls code in a child and checks that it died of SIGSEGV
static void call_faults(const void *code)
{
    fflush(stdout);
    pid_t pid = fork();
    if (pid == 0)
    {
        call(code);
        _exit(0); // it ran: wrong
    }

    int status = -1;
    CHECK(pid > 0 && waitpid(pid, &status, 0) == pid);
    CHECK(WIFSIGNALED(status) && WTERMSIG(status) == SIGSEGV);
}

// Only the program's text runs: its data, its stack and its heap never do
static void test_data_never_runs(void)
{
    // The stack: a child's own array, so the parent's frame is never involved
    fflush(stdout);
    pid_t pid = fork();
    if (pid == 0)
    {
        unsigned char on_stack[CODE_SIZE];
        memcpy(on_stack, rodata_code, CODE_SIZE);
        call(on_stack);
        _exit(0);
    }
    int status = -1;
    CHECK(pid > 0 && waitpid(pid, &status, 0) == pid);
    CHECK(WIFSIGNALED(status) && WTERMSIG(status) == SIGSEGV);

    unsigned char *on_heap = malloc(CODE_SIZE);
    CHECK(on_heap != NULL);
    if (on_heap != NULL)
    {
        memcpy(on_heap, rodata_code, CODE_SIZE);
        call_faults(on_heap);
        free(on_heap);
    }

    call_faults(data_code);
    call_faults(rodata_code);
}

// The JIT pattern: written while writable, run once executable, refused again after
static void test_jit(int nx)
{
    unsigned char *code = map(PAGE, PROT_READ | PROT_WRITE);
    CHECK(code != MAP_FAILED);
    if (code == MAP_FAILED)
        return;
    memcpy(code, rodata_code, CODE_SIZE);

    if (nx)
        call_faults(code);

    CHECK(mprotect(code, PAGE, PROT_READ | PROT_EXEC) == 0);
    CHECK(call(code) == CODE_RESULT);
    faults((char *)code, 1); // and no longer writable

    CHECK(mprotect(code, PAGE, PROT_READ | PROT_WRITE) == 0);
    if (nx)
        call_faults(code);

    CHECK(munmap(code, PAGE) == 0);
}

// Read, write and execute at once (Linux allows it); a fork's copy-on-write copy stays executable
static void test_executable_after_fork(void)
{
    unsigned char *code = map(PAGE, PROT_READ | PROT_WRITE | PROT_EXEC);
    CHECK(code != MAP_FAILED);
    if (code == MAP_FAILED)
        return;
    memcpy(code, rodata_code, CODE_SIZE);
    CHECK(call(code) == CODE_RESULT);

    fflush(stdout);
    pid_t pid = fork();
    if (pid == 0)
    {
        code[1] = 7; // mov eax, 7: the shared page is copied here
        _exit(call(code) == 7 ? 0 : 1);
    }

    int status = -1;
    CHECK(pid > 0 && waitpid(pid, &status, 0) == pid);
    CHECK(WIFEXITED(status) && WEXITSTATUS(status) == 0);
    CHECK(call(code) == CODE_RESULT); // the parent's page is its own
    CHECK(munmap(code, PAGE) == 0);
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
    test_protect_keeps_data();
    test_protect_system_calls();
    test_protect_middle();
    test_guard_page();
    test_protect_after_fork();
    test_protect_errors();

    int nx = cpu_has_nx();
    if (nx)
        test_data_never_runs();
    else
        printf("  no NX on this CPU: data pages run, not checked\n");
    test_jit(nx);
    test_executable_after_fork();
    return test_status();
}
