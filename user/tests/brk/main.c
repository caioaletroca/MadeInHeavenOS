#include <stddef.h>
#include <stdint.h>
#include <unistd.h>
#include <errno.h>
#include <test.h>

/*
 * brk/sbrk and the user stack: heap on the page after .bss, exact break,
 * zeroed pages after shrinking and growing again, failures leave the break
 * alone, and a stack frame larger than the old one-page stack.
 */

#define PAGE_SIZE 4096

static volatile int bss; // the last thing before the heap

// True if every byte of [p, p + n) is zero
static int all_zero(const volatile unsigned char *p, size_t n)
{
    for (size_t i = 0; i < n; i++)
        if (p[i] != 0)
            return 0;
    return 1;
}

static void test_brk(void)
{
    // Initial break: on the page right after .bss
    uintptr_t start = (uintptr_t)sbrk(0);
    CHECK((start & (PAGE_SIZE - 1)) == 0);
    CHECK(start > (uintptr_t)&bss);
    CHECK(start - (uintptr_t)&bss <= PAGE_SIZE);

    // Grow one page: returns the old break, memory is zeroed and writable
    volatile unsigned char *page = sbrk(PAGE_SIZE);
    CHECK((uintptr_t)page == start);
    CHECK((uintptr_t)sbrk(0) == start + PAGE_SIZE);
    CHECK(all_zero(page, PAGE_SIZE));
    page[0] = 0xAA;
    page[PAGE_SIZE - 1] = 0x55;
    CHECK(page[0] == 0xAA && page[PAGE_SIZE - 1] == 0x55);

    // Unaligned break across several pages: the break is exact,
    // the whole last partial page is usable
    CHECK(sbrk(3 * PAGE_SIZE + 100) == (void *)(start + PAGE_SIZE));
    CHECK((uintptr_t)sbrk(0) == start + 4 * PAGE_SIZE + 100);
    volatile unsigned char *last = (unsigned char *)(start + 4 * PAGE_SIZE);
    last[PAGE_SIZE - 1] = 1;
    CHECK(last[PAGE_SIZE - 1] == 1);
    CHECK(page[0] == 0xAA); // growing kept the old contents

    // Shrink back to the start, grow again: fresh zeroed pages
    CHECK(brk((void *)start) == 0);
    CHECK((uintptr_t)sbrk(0) == start);
    CHECK(sbrk(2 * PAGE_SIZE) == (void *)start);
    CHECK(all_zero(page, 2 * PAGE_SIZE));

    // Failures leave the break alone and set ENOMEM
    uintptr_t before = (uintptr_t)sbrk(0);

    errno = 0;
    CHECK(sbrk((intptr_t)1 << 47) == (void *)-1); // past the user half, so past the stack limit
    CHECK(errno == ENOMEM);

    errno = 0;
    CHECK(sbrk(INTPTR_MIN) == (void *)-1); // wraps below 0
    CHECK(errno == ENOMEM);

    errno = 0;
    CHECK(brk((void *)(start - PAGE_SIZE)) == -1); // below the heap start
    CHECK(errno == ENOMEM);

    CHECK((uintptr_t)sbrk(0) == before);

    // Leave the heap empty
    CHECK(brk((void *)start) == 0);
}

static void test_stack(void)
{
    // Half of the 64 KiB stack; one page used to be all there was
    volatile unsigned char buffer[32 * 1024];

    for (size_t i = 0; i < sizeof(buffer); i += PAGE_SIZE)
        buffer[i] = (unsigned char)i;
    buffer[sizeof(buffer) - 1] = 0x77;

    int ok = 1;
    for (size_t i = 0; i < sizeof(buffer); i += PAGE_SIZE)
        if (buffer[i] != (unsigned char)i)
            ok = 0;
    CHECK(ok && buffer[sizeof(buffer) - 1] == 0x77);
}

int main(void)
{
    test_brk();
    test_stack();
    return test_status();
}
