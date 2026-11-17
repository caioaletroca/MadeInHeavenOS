#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

/*
 * Custom new and delete operators for C++.
 */
static void *allocate(size_t size)
{
    void *p = malloc(size == 0 ? 1 : size);
    if (p == NULL)
    {
        fprintf(stderr, "operator new: out of memory\n");
        abort();
    }
    return p;
}

void *operator new(size_t size) { return allocate(size); }
void *operator new[](size_t size) { return allocate(size); }

void operator delete(void *p) noexcept { free(p); }
void operator delete[](void *p) noexcept { free(p); }

// Sized versions (C++14): the compiler passes the object's size; malloc does not need it
void operator delete(void *p, size_t) noexcept { free(p); }
void operator delete[](void *p, size_t) noexcept { free(p); }