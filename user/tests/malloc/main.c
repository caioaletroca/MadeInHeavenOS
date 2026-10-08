#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <test.h>

/*
 * malloc, free, calloc and realloc. The double free check is its own
 * program (malloc_abort): it ends the process.
 */

// sizeof(block_t) in libc/internal/malloc.h: one unit of header per block
#define HEADER 32

// First address of the heap, before any malloc
static uintptr_t heap_base;

static uintptr_t heap_end(void)
{
    return (uintptr_t)sbrk(0);
}

// True if every byte of [p, p + n) equals value
static int all_equal(const unsigned char *p, size_t n, unsigned char value)
{
    for (size_t i = 0; i < n; i++)
        if (p[i] != value)
            return 0;
    return 1;
}

/*
 * With everything freed and merged, the whole heap is one free block: a
 * request for all of it minus one header must fit exactly, at the bottom,
 * without growing the heap. Fragments left anywhere make it grow instead.
 */
static void check_one_free_block(int line)
{
    uintptr_t end = heap_end();
    void *all = malloc(end - heap_base - HEADER);

    if (all != (void *)(heap_base + HEADER) || heap_end() != end)
    {
        test_failures++;
        printf("FAIL line %d: heap is not one free block\n", line);
    }
    free(all);
}

static void test_alignment_and_overlap(void)
{
    static const size_t sizes[] = {1, 7, 15, 16, 17, 31, 32, 33, 100, 255, 256, 1000, 4096, 5000, 20000};
    enum { COUNT = sizeof(sizes) / sizeof(sizes[0]) };
    unsigned char *blocks[COUNT];

    // Every block 16-aligned, filled with its own byte
    for (int i = 0; i < COUNT; i++)
    {
        blocks[i] = malloc(sizes[i]);
        CHECK(blocks[i] != NULL);
        CHECK(((uintptr_t)blocks[i] & 15) == 0);
        memset(blocks[i], i + 1, sizes[i]);
    }

    // No block wrote into another
    for (int i = 0; i < COUNT; i++)
        CHECK(all_equal(blocks[i], sizes[i], i + 1));

    // Free in a mixed order so merges happen in both directions
    for (int i = 0; i < COUNT; i += 2)
        free(blocks[i]);
    for (int i = 1; i < COUNT; i += 2)
        free(blocks[i]);

    check_one_free_block(__LINE__);
}

static void test_reuse(void)
{
    // Freed memory is used again: a thousand rounds never grow the heap
    uintptr_t end = heap_end();
    for (int i = 0; i < 1000; i++)
    {
        void *p = malloc(1024);
        CHECK(p != NULL);
        free(p);
    }
    CHECK(heap_end() == end);
}

static void test_large(void)
{
    // More than the 64 KiB __morecore asks for at a time
    size_t size = 100 * 1024;
    unsigned char *big = malloc(size);
    CHECK(big != NULL);
    CHECK(((uintptr_t)big & 15) == 0);
    big[0] = 1;
    big[size - 1] = 2;
    CHECK(big[0] == 1 && big[size - 1] == 2);
    free(big);

    check_one_free_block(__LINE__);
}

static void test_calloc(void)
{
    // Dirty the whole heap, free it, then calloc must still hand out zeros
    uintptr_t end = heap_end();
    size_t whole = end - heap_base - HEADER;
    unsigned char *dirty = malloc(whole);
    CHECK(dirty != NULL);
    memset(dirty, 0xAB, whole);
    free(dirty);

    unsigned char *clean = calloc(1000, 4);
    CHECK(clean != NULL);
    CHECK(heap_end() == end); // came from the dirty memory, not fresh pages
    CHECK(all_equal(clean, 4000, 0));
    free(clean);
}

static void test_realloc(void)
{
    // NULL behaves like malloc
    unsigned char *p = realloc(NULL, 10);
    CHECK(p != NULL);
    for (int i = 0; i < 10; i++)
        p[i] = (unsigned char)i;

    // Growing within the rounding slack stays in place (10 bytes use a whole unit)
    CHECK(realloc(p, HEADER) == p);

    // Growing beyond it moves the block and keeps the contents
    unsigned char *q = realloc(p, 5000);
    CHECK(q != NULL && q != p);
    int kept = 1;
    for (int i = 0; i < 10; i++)
        if (q[i] != (unsigned char)i)
            kept = 0;
    CHECK(kept);

    // Shrinking stays in place
    CHECK(realloc(q, 100) == q);

    // Size 0 frees and returns NULL
    CHECK(realloc(q, 0) == NULL);

    check_one_free_block(__LINE__);
}

static void test_failures_enomem(void)
{
    uintptr_t end = heap_end();

    CHECK(malloc(0) == NULL);

    errno = 0;
    CHECK(malloc(SIZE_MAX) == NULL); // the size check in malloc
    CHECK(errno == ENOMEM);

    errno = 0;
    CHECK(malloc((size_t)1 << 47) == NULL); // passes malloc, the kernel refuses the break
    CHECK(errno == ENOMEM);

    errno = 0;
    CHECK(calloc(SIZE_MAX / 16 + 2, 16) == NULL); // the product would wrap to 16
    CHECK(errno == ENOMEM);

    CHECK(heap_end() == end);
}

int main(void)
{
    heap_base = heap_end();

    test_alignment_and_overlap();
    test_reuse();
    test_large();
    test_calloc();
    test_realloc();
    test_failures_enomem();
    check_one_free_block(__LINE__);

    // vsnprintf has no length modifiers yet: %u with an unsigned int
    printf("malloc: heap %u KiB\n", (unsigned int)((heap_end() - heap_base) / 1024));
    return test_status();
}
