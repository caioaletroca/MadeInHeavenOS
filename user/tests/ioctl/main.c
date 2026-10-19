#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <test.h>

/*
 * ioctl: TIOCGWINSZ on the console, ENOTTY from everything that is no
 * terminal, and the errors of the syscall layer that copies the argument.
 */

static void test_winsize(void)
{
    // fd 0 is the console: 80x25 text, no pixel size; success leaves errno alone
    struct winsize ws;
    memset(&ws, 0xA5, sizeof(ws));
    errno = 0;
    CHECK(ioctl(0, TIOCGWINSZ, &ws) == 0);
    CHECK(errno == 0);
    CHECK(ws.ws_row == 25 && ws.ws_col == 80);
    CHECK(ws.ws_xpixel == 0 && ws.ws_ypixel == 0); // zeroed by the kernel, not left as 0xA5

    // The same through /dev/console opened by name
    int fd = open("/dev/console", O_RDWR);
    struct winsize again;
    memset(&again, 0, sizeof(again));
    CHECK(ioctl(fd, TIOCGWINSZ, &again) == 0);
    CHECK(memcmp(&ws, &again, sizeof(ws)) == 0);

    // A request the console does not know
    errno = 0;
    CHECK(ioctl(fd, MIHOS_IOR('T', 0xFF, int), &ws) == -1 && errno == ENOTTY);
    close(fd);
}

static void test_not_terminals(void)
{
    struct winsize ws;
    const char *paths[] = {"/usr/tests/hello", "/usr", "/dev/null", "/dev/zero", NULL};

    for (int i = 0; paths[i] != NULL; i++)
    {
        int fd = open(paths[i], O_RDONLY);
        CHECK(fd >= 0);
        errno = 0;
        CHECK(ioctl(fd, TIOCGWINSZ, &ws) == -1 && errno == ENOTTY);
        close(fd);
    }
}

static void test_errors(void)
{
    struct winsize ws;

    // A bad pointer: the kernel's copy out fails
    errno = 0;
    CHECK(ioctl(0, TIOCGWINSZ, (struct winsize *)8) == -1 && errno == EFAULT);

    // An argument bigger than the syscall layer copies
    errno = 0;
    CHECK(ioctl(0, MIHOS_IOC(MIHOS_IOC_READ, 'T', 0x13, 1000), &ws) == -1 && errno == EINVAL);

    // Not open
    errno = 0;
    CHECK(ioctl(99, TIOCGWINSZ, &ws) == -1 && errno == EBADF);
    errno = 0;
    CHECK(ioctl(-1, TIOCGWINSZ, &ws) == -1 && errno == EBADF);
}

int main(void)
{
    test_winsize();
    test_not_terminals();
    test_errors();
    return test_status();
}
