#include <stdlib.h>
#include <stdio.h>

/*
 * free must catch a double free and abort: killed by SIGABRT, which the test
 * runner counts as status 134 (128 + 6). Reaching the end means it went unnoticed.
 */
int main(void)
{
    void *p = malloc(64);
    free(p);
    free(p);

    printf("FAIL: double free was not detected\n");
    return 0;
}
