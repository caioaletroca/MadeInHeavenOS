#include <string.h>

/**
 * Citating a good person:
 * 
 * "GCC is very good at optimizing. It recognizes that your for loop is equivalent to memset,
 * so it replaces the loop with a call to memset. As you've already noticed,
 * that can be problematic when it happens inside your implementation of memset."
 * 
 * The solution was to add the attribute on this function, this may happen in other functions like
 * memcpy, memmove, memset, and memcmp.
 * 
 * Full solution: https://www.reddit.com/r/osdev/comments/1b446qe/gcc_assembled_code_turn_into_a_infinity_loop/
*/
__attribute__((optimize("no-tree-loop-distribute-patterns")))
void *memset(void *str, int c, size_t n) {
    char* buf = (char*) str;

    for(size_t i = 0; i < n; i++) {
        buf[i] = c;
    }

    return str;
}