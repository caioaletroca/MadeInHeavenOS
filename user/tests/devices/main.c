#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <test.h>

/*
 * Character devices in /dev: /dev/null and /dev/zero through open, read,
 * write and close, what stat and readdir say about them, and what they
 * refuse; /dev/console opened by name. isatty on the other devices is in the
 * tty test.
 */

static void test_null(void)
{
    char buffer[1000];
    memset(buffer, 'x', sizeof(buffer));

    // Writes take everything, also more than the kernel's 256-byte chunk
    int fd = open("/dev/null", O_WRONLY);
    CHECK(fd >= 0);
    CHECK(write(fd, buffer, 5) == 5);
    CHECK(write(fd, buffer, sizeof(buffer)) == (ssize_t)sizeof(buffer));
    CHECK(write(fd, buffer, 0) == 0);
    errno = 0;
    CHECK(read(fd, buffer, 1) == -1 && errno == EBADF); // opened for writing only
    CHECK(close(fd) == 0);

    // Reads are at the end, every time
    fd = open("/dev/null", O_RDONLY);
    CHECK(fd >= 0);
    CHECK(read(fd, buffer, sizeof(buffer)) == 0);
    CHECK(read(fd, buffer, sizeof(buffer)) == 0);
    CHECK(buffer[0] == 'x'); // nothing was written into the buffer
    errno = 0;
    CHECK(write(fd, buffer, 1) == -1 && errno == EBADF); // opened for reading only
    CHECK(close(fd) == 0);
}

static void test_zero(void)
{
    char buffer[600];
    memset(buffer, 0xA5, sizeof(buffer));

    int fd = open("/dev/zero", O_RDWR);
    CHECK(fd >= 0);

    // Reads fill the buffer with zeros; a read may be short (a chunk), so loop
    size_t done = 0;
    while (done < 500)
    {
        ssize_t n = read(fd, buffer + done, 500 - done);
        CHECK(n > 0);
        if (n <= 0)
            break;
        done += (size_t)n;
    }

    int zeros = 1;
    for (size_t i = 0; i < 500; i++)
        zeros = zeros && buffer[i] == 0;
    CHECK(zeros);
    CHECK((unsigned char)buffer[500] == 0xA5); // nothing past what was asked

    CHECK(write(fd, buffer, 10) == 10); // writes are swallowed
    CHECK(close(fd) == 0);
}

static void test_stat(void)
{
    // Zeroed: struct stat has padding that libc never writes, and memcmp sees it
    struct stat null, by_fd, zero, dev;
    memset(&null, 0, sizeof(null));
    memset(&by_fd, 0, sizeof(by_fd));

    CHECK(stat("/dev/null", &null) == 0);
    CHECK(S_ISCHR(null.st_mode) && !S_ISREG(null.st_mode) && !S_ISDIR(null.st_mode));
    CHECK((null.st_mode & 0777) == 0666);
    CHECK(major(null.st_rdev) == 1 && minor(null.st_rdev) == 3);
    CHECK(null.st_rdev == makedev(1, 3));
    CHECK(null.st_size == 0);

    CHECK(stat("/dev/zero", &zero) == 0);
    CHECK(S_ISCHR(zero.st_mode) && major(zero.st_rdev) == 1 && minor(zero.st_rdev) == 5);
    CHECK(zero.st_ino != null.st_ino);

    // Both live on devfs: the st_dev of /dev itself
    CHECK(stat("/dev", &dev) == 0);
    CHECK(null.st_dev == dev.st_dev && zero.st_dev == dev.st_dev);

    // fstat of an open device is its stat
    int fd = open("/dev/null", O_RDONLY);
    CHECK(fstat(fd, &by_fd) == 0);
    CHECK(memcmp(&null, &by_fd, sizeof(null)) == 0);
    close(fd);
}

static void test_readdir(void)
{
    struct stat null;
    CHECK(stat("/dev/null", &null) == 0);

    DIR *dir = opendir("/dev");
    CHECK(dir != NULL);
    if (dir == NULL)
        return;

    int seen_null = 0, seen_zero = 0, seen_console = 0, dots = 0, types_ok = 1;
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL)
    {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
        {
            dots++;
            types_ok = types_ok && entry->d_type == DT_DIR;
            continue;
        }

        types_ok = types_ok && (entry->d_type == DT_CHR || entry->d_type == DT_BLK); // disks: the disks test
        if (strcmp(entry->d_name, "null") == 0)
        {
            seen_null++;
            CHECK(entry->d_ino == null.st_ino);
        }
        seen_zero += strcmp(entry->d_name, "zero") == 0;
        seen_console += strcmp(entry->d_name, "console") == 0;
    }
    closedir(dir);

    CHECK(dots == 2 && seen_null == 1 && seen_zero == 1 && seen_console == 1);
    CHECK(types_ok);
}

// /dev/console opens like any device; fds 0-2 are that device
static void test_console(void)
{
    int fd = open("/dev/console", O_RDWR);
    CHECK(fd >= 0);
    CHECK(write(fd, "", 0) == 0); // nothing on the screen
    CHECK(isatty(fd) == 1);

    struct stat opened, fd0;
    CHECK(fstat(fd, &opened) == 0 && fstat(0, &fd0) == 0);
    CHECK(opened.st_ino == fd0.st_ino && opened.st_rdev == fd0.st_rdev);
    CHECK(close(fd) == 0);
}

static void test_refusals(void)
{
    char buffer[64];
    struct stat st;

    int fd = open("/dev/null", O_RDWR);
    CHECK(fd >= 0);
    errno = 0;
    CHECK(lseek(fd, 0, SEEK_SET) == -1 && errno == ESPIPE); // a stream, not a file with positions
    errno = 0;
    CHECK(getdents(fd, buffer, sizeof(buffer)) == -1 && errno == ENOTDIR);
    close(fd);

    errno = 0;
    CHECK(opendir("/dev/null") == NULL && errno == ENOTDIR);
    errno = 0;
    CHECK(open("/dev/zero", O_RDONLY | O_DIRECTORY) == -1 && errno == ENOTDIR);
    errno = 0;
    CHECK(stat("/dev/null/", &st) == -1 && errno == ENOTDIR);
    errno = 0;
    CHECK(stat("/dev/null/x", &st) == -1 && errno == ENOTDIR);
    errno = 0;
    CHECK(open("/dev/nope", O_RDONLY) == -1 && errno == ENOENT);

    // Write access is still refused outside /dev
    errno = 0;
    CHECK(open("/usr/tests/hello", O_WRONLY) == -1 && errno == EROFS);
}

int main(void)
{
    test_null();
    test_zero();
    test_stat();
    test_readdir();
    test_console();
    test_refusals();

    // Every fd was closed: the next open gets fd 3 again
    int fd = open("/dev/null", O_RDONLY);
    CHECK(fd == 3);
    close(fd);

    return test_status();
}
