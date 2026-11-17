#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <test.h>

/*
 * malloc, free, calloc and realloc: what C and POSIX promise, on any
 * allocator. Not how the heap is laid out (ours grows the break, mlibc maps
 * memory), not malloc(0) and realloc(p, 0) (implementation-defined), not a
 * double free (undefined: ours catches it and aborts, mlibc does not).
 */

// True if every byte of [p, p + n) equals value
static int all_equal(const unsigned char *p, size_t n, unsigned char value)
{
    for (size_t i = 0; i < n; i++)
        if (p[i] != value)
            return 0;
    return 1;
}

/*
 * The alignment C promises a block of size bytes: enough for any object that
 * fits in it (C23's wording), so 16 (max_align_t) from 16 bytes on and the
 * largest power of two up to the size below that.
 */
static uintptr_t promised_alignment(size_t size)
{
    uintptr_t alignment = 1;
    while (alignment < _Alignof(max_align_t) && alignment * 2 <= size)
        alignment *= 2;
    return alignment;
}

static void test_alignment_and_overlap(void)
{
    static const size_t sizes[] = {1, 7, 15, 16, 17, 31, 32, 33, 100, 255, 256, 1000, 4096, 5000, 20000};
    enum { COUNT = sizeof(sizes) / sizeof(sizes[0]) };
    unsigned char *blocks[COUNT];

    // Every block aligned as promised, filled with its own byte
    for (int i = 0; i < COUNT; i++)
    {
        blocks[i] = malloc(sizes[i]);
        CHECK(blocks[i] != NULL);
        if (blocks[i] == NULL)
            return;
        if (((uintptr_t)blocks[i] & (promised_alignment(sizes[i]) - 1)) != 0)
            printf("  malloc(%zu) gave %p\n", sizes[i], (void *)blocks[i]);
        CHECK(((uintptr_t)blocks[i] & (promised_alignment(sizes[i]) - 1)) == 0);
        memset(blocks[i], i + 1, sizes[i]);
    }

    // No block wrote into another
    for (int i = 0; i < COUNT; i++)
        CHECK(all_equal(blocks[i], sizes[i], i + 1));

    // Freed in a mixed order
    for (int i = 0; i < COUNT; i += 2)
        free(blocks[i]);
    for (int i = 1; i < COUNT; i += 2)
        free(blocks[i]);
}

static void test_reuse(void)
{
    // A thousand rounds of malloc and free keep working (freed memory goes back)
    for (int i = 0; i < 1000; i++)
    {
        unsigned char *p = malloc(1024);
        CHECK(p != NULL);
        if (p == NULL)
            return;
        p[0] = p[1023] = (unsigned char)i;
        free(p);
    }
}

static void test_large(void)
{
    // Bigger than any small-block class, and than ours asks the kernel for at a time
    size_t size = 100 * 1024;
    unsigned char *big = malloc(size);
    CHECK(big != NULL);
    if (big == NULL)
        return;
    CHECK(((uintptr_t)big & (_Alignof(max_align_t) - 1)) == 0);
    big[0] = 1;
    big[size - 1] = 2;
    CHECK(big[0] == 1 && big[size - 1] == 2);
    free(big);
}

static void test_calloc(void)
{
    // Dirty memory, freed, then calloc of the same size must still hand out zeros
    for (int round = 0; round < 8; round++)
    {
        unsigned char *dirty = malloc(4000);
        CHECK(dirty != NULL);
        if (dirty == NULL)
            return;
        memset(dirty, 0xAB, 4000);
        free(dirty);

        unsigned char *clean = calloc(1000, 4);
        CHECK(clean != NULL);
        if (clean == NULL)
            return;
        CHECK(all_equal(clean, 4000, 0));
        free(clean);
    }
}

static void test_realloc(void)
{
    // NULL behaves like malloc
    unsigned char *p = realloc(NULL, 10);
    CHECK(p != NULL);
    if (p == NULL)
        return;
    for (int i = 0; i < 10; i++)
        p[i] = (unsigned char)i;

    // Growing keeps the contents, wherever the block ends up
    unsigned char *q = realloc(p, 5000);
    CHECK(q != NULL);
    if (q == NULL)
    {
        free(p);
        return;
    }
    int kept = 1;
    for (int i = 0; i < 10; i++)
        if (q[i] != (unsigned char)i)
            kept = 0;
    CHECK(kept);
    q[4999] = 0x5A;

    // Shrinking keeps what fits
    unsigned char *r = realloc(q, 5);
    CHECK(r != NULL);
    if (r == NULL)
    {
        free(q);
        return;
    }
    CHECK(r[0] == 0 && r[4] == 4);
    free(r);
}

static void test_failures_enomem(void)
{
    // Sizes no allocator can give: NULL and ENOMEM (POSIX), never a small block
    errno = 0;
    CHECK(malloc(SIZE_MAX) == NULL);
    CHECK(errno == ENOMEM);

    errno = 0;
    CHECK(malloc((size_t)1 << 47) == NULL); // the whole user half
    CHECK(errno == ENOMEM);

    errno = 0;
    CHECK(calloc(SIZE_MAX / 16 + 2, 16) == NULL); // the product would wrap to 16
    CHECK(errno == ENOMEM);
}

int main(void)
{
    test_alignment_and_overlap();
    test_reuse();
    test_large();
    test_calloc();
    test_realloc();
    test_failures_enomem();
    return test_status();
}
