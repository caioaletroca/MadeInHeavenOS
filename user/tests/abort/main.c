#include <stdlib.h>
#include <stdio.h>

/*
 * abort ends the program with SIGABRT (C): the test runner counts that as
 * status 134 (128 + 6), and the shell test reports it as killed by signal 6.
 * Reaching the end means abort returned.
 */
int main(void)
{
    abort();

    printf("FAIL: abort returned\n");
    return 0;
}
