#include <stddef.h>
#include <unistd.h>
#include <stdio.h>

/*
 * First ELF program: prints a line and exits with 42, so the exit status
 * the kernel reports visibly comes from the program.
 *
 * The status also checks the loader: 42 comes from .data (copied from the
 * file) plus a .bss variable that must be zero (only in memory). Missing
 * data gives 0, an unzeroed .bss gives anything but 42.
 */

// volatile: keep the compiler from folding them into a constant 42
static volatile int status = 42; // .data
static volatile int zero;        // .bss

int main(void)
{
    printf("Hello from an ELF program!\n");
    return status + zero;
}
