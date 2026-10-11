#include <sys/reboot.h>
#include <errno.h>
#include <test.h>

/*
 * Only init (pid 1) may halt the system. If the check is missing, this test
 * halts the machine instead of failing: a very visible failure.
 */
int main(void)
{
    errno = 0;
    CHECK(reboot(RB_HALT_SYSTEM) == -1 && errno == EPERM);

    // Permission comes first: an unknown command is refused the same way
    errno = 0;
    CHECK(reboot(12345) == -1 && errno == EPERM);

    return test_status();
}
