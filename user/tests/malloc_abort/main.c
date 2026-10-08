#include <stdlib.h>
#include <stdio.h>

/*
 * free must catch a double free and abort: the expected exit status is 134
 * (128 + SIGABRT, see grub/grub.cfg). Reaching the end means it went unnoticed.
 */
int main(void)
{
    void *p = malloc(64);
    free(p);
    free(p);

    printf("FAIL: double free was not detected\n");
    return 0;
}
