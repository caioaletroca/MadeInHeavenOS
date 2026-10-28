#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>
#include <test.h>

/*
 * Files through the VFS: open, read at an advancing offset, lseek, close, and
 * the errors around them. Reads /usr/tests/hello (any program would do).
 */

#define FILE_PATH "/usr/tests/hello"
#define FILES_MAX 16 // PROCESS_MAX_FILES in the kernel

// Reads the whole file in pieces of `piece` bytes; returns the total, or -1
static long read_all(int fd, size_t piece)
{
    char buffer[300];
    long total = 0;

    for (;;)
    {
        ssize_t n = read(fd, buffer, piece);
        if (n < 0)
            return -1;
        if (n == 0)
            return total;
        total += n;
    }
}

static void test_open_read(void)
{
    int fd = open(FILE_PATH, O_RDONLY);
    CHECK(fd == 3); // the lowest free fd after stdin, stdout, stderr

    unsigned char header[4];
    CHECK(read(fd, header, 4) == 4);
    CHECK(memcmp(header, "\x7F" "ELF", 4) == 0);

    // The offset moved: the next read continues after the header
    off_t size = lseek(fd, 0, SEEK_END);
    CHECK(size > 64);
    CHECK(lseek(fd, 4, SEEK_SET) == 4);

    // Reading in odd-sized pieces, then in big ones, adds up to the size
    CHECK(read_all(fd, 7) == size - 4);
    CHECK(read(fd, header, 4) == 0); // at the end: 0, again and again
    CHECK(read(fd, header, 4) == 0);
    CHECK(lseek(fd, 0, SEEK_SET) == 0);
    CHECK(read_all(fd, 300) == size);

    CHECK(close(fd) == 0);
}

static void test_lseek(void)
{
    int fd = open(FILE_PATH, O_RDONLY);
    off_t size = lseek(fd, 0, SEEK_END);

    CHECK(lseek(fd, 10, SEEK_SET) == 10);
    CHECK(lseek(fd, 5, SEEK_CUR) == 15);
    CHECK(lseek(fd, -5, SEEK_CUR) == 10);
    CHECK(lseek(fd, 0, SEEK_CUR) == 10); // the usual way to ask for the position
    CHECK(lseek(fd, -1, SEEK_END) == size - 1);

    // The byte at an offset is the same however it was reached
    unsigned char a, b;
    CHECK(lseek(fd, 100, SEEK_SET) == 100 && read(fd, &a, 1) == 1);
    CHECK(lseek(fd, -(size - 100), SEEK_END) == 100 && read(fd, &b, 1) == 1);
    CHECK(a == b);

    // Past the end is allowed; reading there gives 0
    CHECK(lseek(fd, size + 1000, SEEK_SET) == size + 1000);
    CHECK(read(fd, &a, 1) == 0);

    // Before the start is not, and a failed lseek leaves the offset alone
    CHECK(lseek(fd, 20, SEEK_SET) == 20);
    errno = 0;
    CHECK(lseek(fd, -21, SEEK_CUR) == -1 && errno == EINVAL);
    errno = 0;
    CHECK(lseek(fd, -1, SEEK_SET) == -1 && errno == EINVAL);
    errno = 0;
    CHECK(lseek(fd, INT64_MIN, SEEK_END) == -1 && errno == EINVAL);
    errno = 0;
    CHECK(lseek(fd, 0, 3) == -1 && errno == EINVAL); // no such whence
    CHECK(lseek(fd, 0, SEEK_CUR) == 20);

    // The console has no positions
    errno = 0;
    CHECK(lseek(0, 0, SEEK_SET) == -1 && errno == ESPIPE);
    errno = 0;
    CHECK(lseek(42, 0, SEEK_SET) == -1 && errno == EBADF);

    close(fd);
}

static void test_independent_offsets(void)
{
    // Two opens are two open files: each has its own offset
    int first = open(FILE_PATH, O_RDONLY);
    int second = open(FILE_PATH, O_RDONLY);
    CHECK(first >= 0 && second >= 0 && first != second);

    unsigned char x[8], y[8];
    CHECK(read(first, x, 8) == 8);
    CHECK(lseek(second, 0, SEEK_CUR) == 0);
    CHECK(read(second, y, 8) == 8);
    CHECK(memcmp(x, y, 8) == 0);

    close(first);
    close(second);
}

static void test_errors(void)
{
    char byte;

    errno = 0;
    CHECK(open("/nope", O_RDONLY) == -1 && errno == ENOENT);
    errno = 0;
    CHECK(open("usr/tests/hello", O_RDONLY) == -1 && errno == ENOENT); // relative
    errno = 0;
    CHECK(open(FILE_PATH "/", O_RDONLY) == -1 && errno == ENOTDIR);
    errno = 0;
    CHECK(open(FILE_PATH, O_WRONLY) == -1 && errno == EROFS);
    errno = 0;
    CHECK(open(FILE_PATH, O_RDWR) == -1 && errno == EROFS);
    errno = 0;
    CHECK(open(FILE_PATH, 0x100) == -1 && errno == EINVAL); // unknown flag
    errno = 0;
    CHECK(open((const char *)1, O_RDONLY) == -1 && errno == EFAULT);
    errno = 0;
    // Kernel memory (the higher half) is refused too, never printed
    CHECK(write(1, (const void *)0xFFFFFFFF80100000, 16) == -1 && errno == EFAULT);

    // A path longer than PATH_MAX
    static char long_path[5000];
    memset(long_path, 'a', sizeof(long_path) - 1);
    long_path[0] = '/';
    errno = 0;
    CHECK(open(long_path, O_RDONLY) == -1 && errno == ENAMETOOLONG);

    // Opened read-only: writing is refused
    int fd = open(FILE_PATH, O_RDONLY);
    errno = 0;
    CHECK(write(fd, "x", 1) == -1 && errno == EBADF);
    close(fd);

    // Directories open (readdir will need it), but cannot be read as bytes
    fd = open("/usr/tests", O_RDONLY);
    CHECK(fd >= 0);
    errno = 0;
    CHECK(read(fd, &byte, 1) == -1 && errno == EISDIR);
    close(fd);

    // A closed fd is gone
    fd = open(FILE_PATH, O_RDONLY);
    CHECK(close(fd) == 0);
    errno = 0;
    CHECK(read(fd, &byte, 1) == -1 && errno == EBADF);
}

static void test_table_full(void)
{
    // 3 are taken (stdin, stdout, stderr): 13 opens fill the table
    int fds[FILES_MAX];
    int opened = 0;
    while (opened < FILES_MAX)
    {
        int fd = open(FILE_PATH, O_RDONLY);
        if (fd < 0)
            break;
        fds[opened++] = fd;
    }
    CHECK(opened == FILES_MAX - 3);

    errno = 0;
    CHECK(open(FILE_PATH, O_RDONLY) == -1 && errno == EMFILE);

    // Closing frees the slots, and the lowest free one is reused first
    for (int i = 0; i < opened; i++)
        CHECK(close(fds[i]) == 0);
    CHECK(open(FILE_PATH, O_RDONLY) == 3);
    close(3);
}

int main(void)
{
    test_open_read();
    test_lseek();
    test_independent_offsets();
    test_errors();
    test_table_full();
    return test_status();
}
