#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <test.h>

/*
 * dup and dup2: a copy is a second fd for the same open file (one offset),
 * the lowest free fd, dup2 onto an open fd closing it, dup2(fd, fd), the
 * errors, a full table, and stdout sent to /dev/null and back, the way the
 * pipe tests will redirect it.
 */

#define FILE_PATH "/usr/tests/hello"
#define FILES_MAX 16 // PROCESS_MAX_FILES in the kernel

// fd is open on the node at path: same inode on the same file system
static int fd_is(int fd, const char *path)
{
    struct stat by_fd, by_path;
    return fstat(fd, &by_fd) == 0 && stat(path, &by_path) == 0 && by_fd.st_ino == by_path.st_ino &&
           by_fd.st_dev == by_path.st_dev;
}

static void test_shared_offset(void)
{
    int fd = open(FILE_PATH, O_RDONLY);
    int copy = dup(fd);
    CHECK(fd == 3 && copy == 4); // the lowest free fd
    CHECK(fd_is(copy, FILE_PATH));

    // One open file: moving one fd's offset moves the other's
    CHECK(lseek(fd, 100, SEEK_SET) == 100);
    CHECK(lseek(copy, 0, SEEK_CUR) == 100);
    char buffer[4];
    CHECK(read(copy, buffer, 4) == 4);
    CHECK(lseek(fd, 0, SEEK_CUR) == 104);

    // Closing one leaves the other working
    CHECK(close(fd) == 0);
    CHECK(lseek(copy, 0, SEEK_SET) == 0 && read(copy, buffer, 4) == 4 && memcmp(buffer, "\x7F" "ELF", 4) == 0);
    CHECK(close(copy) == 0);
}

static void test_lowest_free(void)
{
    int first = open(FILE_PATH, O_RDONLY);
    int second = open(FILE_PATH, O_RDONLY);
    CHECK(first == 3 && second == 4);

    // 3 is free again: dup takes it, not 5
    close(first);
    int copy = dup(second);
    CHECK(copy == 3);
    close(copy);
    close(second);
}

static void test_dup2(void)
{
    int file = open(FILE_PATH, O_RDONLY);
    int null = open("/dev/null", O_RDONLY);
    CHECK(file == 3 && null == 4);

    // Onto an open fd: it is closed, and now reads the file at the shared offset
    CHECK(dup2(file, null) == null);
    CHECK(fd_is(null, FILE_PATH));
    char buffer[4];
    CHECK(read(null, buffer, 4) == 4 && memcmp(buffer, "\x7F" "ELF", 4) == 0);
    CHECK(lseek(file, 0, SEEK_CUR) == 4);

    // Onto itself: nothing happens, the fd stays open
    CHECK(dup2(file, file) == file);
    CHECK(fd_is(file, FILE_PATH));

    // Onto a free fd past the others: no need for the lowest
    CHECK(dup2(file, 10) == 10);
    CHECK(fd_is(10, FILE_PATH));
    CHECK(lseek(10, 0, SEEK_CUR) == 4);

    close(10);
    close(null);
    close(file);
}

static void test_errors(void)
{
    int fd = open(FILE_PATH, O_RDONLY);

    errno = 0;
    CHECK(dup(99) == -1 && errno == EBADF);
    errno = 0;
    CHECK(dup(-1) == -1 && errno == EBADF);
    errno = 0;
    CHECK(dup(5) == -1 && errno == EBADF); // in the table, not open

    errno = 0;
    CHECK(dup2(99, 5) == -1 && errno == EBADF);
    errno = 0;
    CHECK(dup2(fd, FILES_MAX) == -1 && errno == EBADF);
    errno = 0;
    CHECK(dup2(fd, -1) == -1 && errno == EBADF);
    errno = 0;
    CHECK(dup2(5, 5) == -1 && errno == EBADF); // oldfd is checked even when equal (POSIX)

    // A refused dup2 leaves newfd open
    errno = 0;
    CHECK(dup2(5, fd) == -1 && errno == EBADF);
    CHECK(fd_is(fd, FILE_PATH));

    close(fd);
    errno = 0;
    CHECK(dup(fd) == -1 && errno == EBADF); // closed
}

static void test_table_full(void)
{
    // 3 are taken (stdin, stdout, stderr): 13 copies of stdin fill the table
    int fds[FILES_MAX];
    int copies = 0;
    while (copies < FILES_MAX)
    {
        int fd = dup(0);
        if (fd < 0)
            break;
        fds[copies++] = fd;
    }
    CHECK(copies == FILES_MAX - 3);

    errno = 0;
    CHECK(dup(0) == -1 && errno == EMFILE);
    CHECK(dup2(0, fds[0]) == fds[0]); // dup2 onto a used slot needs no free one

    for (int i = 0; i < copies; i++)
        CHECK(close(fds[i]) == 0);
}

/*
 * stdout to /dev/null and back. Nothing goes through stdio while fd 1 is
 * replaced (CHECK's printf would vanish, and stdout's buffering is decided at
 * its first write): the results are kept and checked after the restore.
 */
static void test_redirect_stdout(void)
{
    int saved = dup(1);
    int null = open("/dev/null", O_WRONLY);
    CHECK(saved == 3 && null == 4);

    int moved = dup2(null, 1) == 1;
    close(null);
    struct stat st;
    int is_null = fstat(1, &st) == 0 && S_ISCHR(st.st_mode) && st.st_rdev == makedev(1, 3);
    int written = write(1, "lost\n", 5) == 5;
    int restored = dup2(saved, 1) == 1;
    close(saved);

    CHECK(moved && is_null && written && restored);
    CHECK(fstat(1, &st) == 0 && S_ISCHR(st.st_mode) && st.st_rdev == makedev(5, 1)); // the console again
    CHECK(isatty(1));
}

int main(void)
{
    test_shared_offset();
    test_lowest_free();
    test_dup2();
    test_errors();
    test_table_full();
    test_redirect_stdout();

    // Every fd was closed: the next open gets fd 3 again
    int fd = open("/", O_RDONLY);
    CHECK(fd == 3);
    close(fd);

    return test_status();
}
