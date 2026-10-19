#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <test.h>

/*
 * stat and fstat: what they report for files, directories and the console,
 * that they agree with lseek, readdir and each other, and their errors.
 */

#define FILE_PATH "/usr/tests/hello"
#define MTIME_MIN 1700000000 // a plausible build time: after 2023

// d_ino of `name` in directory `path`, or 0
static ino_t dirent_ino(const char *path, const char *name)
{
    DIR *dir = opendir(path);
    if (dir == NULL)
        return 0;

    ino_t ino = 0;
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL)
        if (strcmp(entry->d_name, name) == 0)
            ino = entry->d_ino;

    closedir(dir);
    return ino;
}

static void test_file(void)
{
    // Zeroed: struct stat has padding after st_mode that libc never writes, and memcmp sees it
    struct stat st, by_fd;
    memset(&st, 0, sizeof(st));
    memset(&by_fd, 0, sizeof(by_fd));

    CHECK(stat(FILE_PATH, &st) == 0);
    CHECK(S_ISREG(st.st_mode) && !S_ISDIR(st.st_mode) && !S_ISCHR(st.st_mode));
    CHECK((st.st_mode & 0777) == 0755); // as packed into the initrd
    CHECK(st.st_nlink == 1);
    CHECK(st.st_ino != 0 && st.st_ino == dirent_ino("/usr/tests", "hello"));
    CHECK(st.st_blocks == (st.st_size + 511) / 512);
    CHECK(st.st_blksize > 0);

    // The pre-2008 names reach the same fields
    CHECK(st.st_mtime == st.st_mtim.tv_sec && st.st_mtime >= MTIME_MIN);
    CHECK(st.st_atime == st.st_mtime && st.st_ctime == st.st_mtime);

    // The size is what lseek finds, and fstat on the open file agrees with stat
    int fd = open(FILE_PATH, O_RDONLY);
    CHECK(fd >= 0);
    CHECK(lseek(fd, 0, SEEK_END) == st.st_size);

    CHECK(fstat(fd, &by_fd) == 0);
    CHECK(memcmp(&st, &by_fd, sizeof(st)) == 0);
    close(fd);

    // Two paths to one file: one inode
    struct stat other;
    CHECK(stat("/usr/../usr/tests/./hello", &other) == 0);
    CHECK(other.st_ino == st.st_ino && other.st_dev == st.st_dev);
}

static void test_directories(void)
{
    struct stat st;
    CHECK(stat("/usr/tests", &st) == 0);
    CHECK(S_ISDIR(st.st_mode) && !S_ISREG(st.st_mode));
    CHECK((st.st_mode & 0777) == 0755);
    CHECK(st.st_ino == dirent_ino("/usr", "tests"));
    CHECK(stat("/usr/tests/", &st) == 0 && S_ISDIR(st.st_mode)); // a trailing '/' on a directory

    // The root, and a directory the archive never lists, still have permissions
    CHECK(stat("/", &st) == 0 && S_ISDIR(st.st_mode) && (st.st_mode & 0777) == 0755);
    CHECK(st.st_ino == dirent_ino("/", "."));
    CHECK(stat("/usr", &st) == 0 && S_ISDIR(st.st_mode) && (st.st_mode & 0777) == 0755);

    // fstat of an open directory
    int fd = open("/usr/tests", O_RDONLY | O_DIRECTORY);
    struct stat by_fd;
    CHECK(fstat(fd, &by_fd) == 0 && S_ISDIR(by_fd.st_mode));
    CHECK(by_fd.st_ino == dirent_ino("/usr", "tests"));
    close(fd);
}

static void test_console(void)
{
    // fds 0-2 are /dev/console: a character device (5:1) on devfs
    struct stat console, st;
    CHECK(stat("/dev/console", &console) == 0);
    CHECK(S_ISCHR(console.st_mode) && (console.st_mode & 0777) == 0600);
    CHECK(console.st_rdev == makedev(5, 1));

    for (int fd = 0; fd <= 2; fd++)
    {
        CHECK(fstat(fd, &st) == 0);
        CHECK(S_ISCHR(st.st_mode) && !S_ISREG(st.st_mode) && !S_ISDIR(st.st_mode));
        CHECK(st.st_size == 0);
        CHECK(st.st_ino == console.st_ino && st.st_dev == console.st_dev && st.st_rdev == console.st_rdev);
    }
}

static void test_devices(void)
{
    // Every file system has its own st_dev: the initrd's files share one, /dev (devfs) has another
    struct stat root, init, dev, up;
    CHECK(stat("/", &root) == 0 && stat("/sbin/init", &init) == 0 && stat("/dev", &dev) == 0);
    CHECK(root.st_dev != 0 && root.st_dev == init.st_dev);
    CHECK(dev.st_dev != 0 && dev.st_dev != root.st_dev);
    CHECK(S_ISDIR(dev.st_mode) && (dev.st_mode & 0777) == 0755);

    // ".." at devfs's root leads back to the initrd's root
    CHECK(stat("/dev/..", &up) == 0 && up.st_dev == root.st_dev && up.st_ino == root.st_ino);
}

static void test_errors(void)
{
    struct stat st;

    errno = 0;
    CHECK(stat("/nope", &st) == -1 && errno == ENOENT);
    errno = 0;
    CHECK(stat("usr", &st) == -1 && errno == ENOENT); // relative: no current directory yet
    errno = 0;
    CHECK(stat("/usr/tests/hello/", &st) == -1 && errno == ENOTDIR);
    errno = 0;
    CHECK(stat("/sbin/init/x", &st) == -1 && errno == ENOTDIR);

    static char long_path[5000];
    memset(long_path, 'a', sizeof(long_path) - 1);
    long_path[0] = '/';
    errno = 0;
    CHECK(stat(long_path, &st) == -1 && errno == ENAMETOOLONG);

    errno = 0;
    CHECK(stat((const char *)8, &st) == -1 && errno == EFAULT);

    errno = 0;
    CHECK(fstat(99, &st) == -1 && errno == EBADF);
    errno = 0;
    CHECK(fstat(-1, &st) == -1 && errno == EBADF);

    int fd = open(FILE_PATH, O_RDONLY);
    close(fd);
    errno = 0;
    CHECK(fstat(fd, &st) == -1 && errno == EBADF);
}

int main(void)
{
    test_file();
    test_directories();
    test_console();
    test_devices();
    test_errors();
    return test_status();
}
