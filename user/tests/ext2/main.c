#include <sys/stat.h>
#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>
#include <test.h>

/*
 * The ext2 test disk (disk.img) mounted on /mnt, from user space: listings,
 * stat, reads through open/lseek/read (big.txt across direct, single and
 * double indirect blocks, the sparse file's hole), overwrites in place, and
 * what is refused.
 */

#define MNT "/mnt"
#define BIG_LINES 38400
#define BIG_SIZE (BIG_LINES * 8) // lines "%07d\n": byte offset = 8 * line
#define SPARSE_SIZE 16387
#define LONG_NAME_LENGTH 255

static char long_name[LONG_NAME_LENGTH + 1];
static char long_path[sizeof(MNT "/") + LONG_NAME_LENGTH];
static char big[BIG_SIZE + 1];

// read() moves at most one 256-byte chunk per call: loop until count bytes or the end
static ssize_t read_full(int fd, void *buffer, size_t count)
{
    size_t done = 0;
    while (done < count)
    {
        ssize_t n = read(fd, (char *)buffer + done, count - done);
        if (n < 0)
            return -1;
        if (n == 0)
            break;
        done += (size_t)n;
    }
    return (ssize_t)done;
}

static int line_ok(const char *p, unsigned int line)
{
    unsigned int value = 0;
    for (int i = 0; i < 7; i++)
    {
        if (p[i] < '0' || p[i] > '9')
            return 0;
        value = value * 10 + (unsigned int)(p[i] - '0');
    }
    return p[7] == '\n' && value == line;
}

// A whole small file equals `text`
static void expect_text(const char *path, const char *text)
{
    char buffer[64];
    int fd = open(path, O_RDONLY);
    CHECK(fd >= 0);
    if (fd < 0)
        return;
    ssize_t n = read_full(fd, buffer, sizeof(buffer));
    CHECK(n == (ssize_t)strlen(text) && memcmp(buffer, text, (size_t)n) == 0);
    close(fd);
}

static void test_listing(void)
{
    static const char *const names[] = {".", "..", "lost+found", long_name, "big.txt",
                                         "docs", "empty", "grow", "hello.txt", "sparse"};
    static const unsigned char types[] = {DT_DIR, DT_DIR, DT_DIR, DT_REG, DT_REG, DT_DIR, DT_REG, DT_DIR, DT_REG, DT_REG};
    const int count = sizeof(names) / sizeof(names[0]);

    DIR *dir = opendir(MNT);
    CHECK(dir != NULL);
    if (dir == NULL)
        return;

    int i = 0, in_order = 1, link_seen = 0;
    ino_t hello_ino = 0;
    errno = 0;
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL)
    {
        if (i < count)
            in_order = in_order && strcmp(entry->d_name, names[i]) == 0 && entry->d_type == types[i];
        if (i < 2)
            in_order = in_order && entry->d_ino == 2; // "." and ".." of the root: inode 2
        link_seen += strcmp(entry->d_name, "link") == 0;
        if (strcmp(entry->d_name, "hello.txt") == 0)
            hello_ino = entry->d_ino;
        i++;
    }
    CHECK(errno == 0); // the end, not an error
    CHECK(i == count && in_order);
    CHECK(link_seen == 0); // a symlink: the VFS cannot show it yet
    closedir(dir);

    struct stat st;
    CHECK(stat(MNT "/hello.txt", &st) == 0 && st.st_ino == hello_ino);

    // A nested directory, and ".." inside the file system
    dir = opendir(MNT "/docs/deep/nested");
    CHECK(dir != NULL);
    if (dir != NULL)
    {
        int files = 0;
        while ((entry = readdir(dir)) != NULL)
            files += strcmp(entry->d_name, "file.txt") == 0 && entry->d_type == DT_REG;
        CHECK(files == 1);
        closedir(dir);
    }

    // Out of the mount through ".."
    dir = opendir(MNT "/..");
    CHECK(dir != NULL);
    if (dir != NULL)
    {
        int usr = 0;
        while ((entry = readdir(dir)) != NULL)
            usr += strcmp(entry->d_name, "usr") == 0;
        CHECK(usr == 1);
        closedir(dir);
    }
}

static void test_stat(void)
{
    struct stat mnt, root, dev, hello, again, big_st, empty;
    CHECK(stat(MNT, &mnt) == 0 && stat("/", &root) == 0 && stat("/dev", &dev) == 0);
    CHECK(S_ISDIR(mnt.st_mode) && (mnt.st_mode & 07777) == 0755);
    CHECK(mnt.st_ino == 2);
    CHECK(mnt.st_nlink == 5); // ".", the entry in its parent, and ".." of lost+found, docs and grow
    CHECK(mnt.st_dev != root.st_dev && mnt.st_dev != dev.st_dev && mnt.st_dev != 0);
    CHECK(mnt.st_mtime >= 1700000000);

    CHECK(stat(MNT "/hello.txt", &hello) == 0 && S_ISREG(hello.st_mode) && hello.st_nlink == 1);
    CHECK(hello.st_size == 21 && hello.st_dev == mnt.st_dev);
    CHECK(stat(MNT "/docs/deep/../../hello.txt", &again) == 0 && again.st_ino == hello.st_ino);

    CHECK(stat(MNT "/big.txt", &big_st) == 0 && big_st.st_size == BIG_SIZE && (big_st.st_mode & 07777) == 0644);
    CHECK(stat(MNT "/empty", &empty) == 0 && empty.st_size == 0);

    // fstat of the open file says the same
    int fd = open(MNT "/big.txt", O_RDONLY);
    CHECK(fd >= 0);
    struct stat opened;
    memset(&opened, 0, sizeof(opened));
    memset(&big_st, 0, sizeof(big_st));
    CHECK(stat(MNT "/big.txt", &big_st) == 0 && fstat(fd, &opened) == 0);
    CHECK(memcmp(&big_st, &opened, sizeof(opened)) == 0);
    close(fd);
}

static void test_reads(void)
{
    expect_text(MNT "/hello.txt", "Hello from the disk!\n");
    expect_text(MNT "/docs/deep/nested/file.txt", "Three directories down.\n");
    expect_text(long_path, "The longest name.\n");

    int fd = open(MNT "/empty", O_RDONLY);
    char byte = 'x';
    CHECK(fd >= 0 && read(fd, &byte, 1) == 0 && byte == 'x');
    close(fd);

    // big.txt whole, through 256-byte reads: every line in its place
    fd = open(MNT "/big.txt", O_RDONLY);
    CHECK(fd >= 0);
    if (fd < 0)
        return;
    big[BIG_SIZE] = 'x';
    CHECK(read_full(fd, big, BIG_SIZE + 1) == BIG_SIZE && big[BIG_SIZE] == 'x');
    int lines_ok = 1;
    for (unsigned int line = 0; line < BIG_LINES; line++)
        lines_ok = lines_ok && line_ok(big + 8 * line, line);
    CHECK(lines_ok);
    CHECK(read(fd, &byte, 1) == 0);

    // Seeking: a line in each kind of block, the end
    static const unsigned int lines[] = {0, 127, 128, 1535, 1536, 34303, 34304, 38399};
    for (unsigned int i = 0; i < sizeof(lines) / sizeof(lines[0]); i++)
    {
        char text[8];
        CHECK(lseek(fd, (off_t)lines[i] * 8, SEEK_SET) == (off_t)lines[i] * 8);
        CHECK(read(fd, text, 8) == 8 && line_ok(text, lines[i]));
    }
    CHECK(lseek(fd, 0, SEEK_END) == BIG_SIZE);
    char tail[16];
    CHECK(lseek(fd, BIG_SIZE - 5, SEEK_SET) == BIG_SIZE - 5 && read(fd, tail, sizeof(tail)) == 5);
    CHECK(memcmp(tail, "8399\n", 5) == 0);
    close(fd);

    // The sparse file: its hole reads as zeros
    fd = open(MNT "/sparse", O_RDONLY);
    CHECK(fd >= 0);
    memset(big, 'x', SPARSE_SIZE + 1);
    CHECK(read_full(fd, big, SPARSE_SIZE + 1) == SPARSE_SIZE);
    CHECK(memcmp(big, "start", 5) == 0 && memcmp(big + 16384, "end", 3) == 0 && big[SPARSE_SIZE] == 'x');
    int zeros = 1;
    for (int i = 5; i < 16384; i++)
        zeros = zeros && big[i] == 0;
    CHECK(zeros);
    close(fd);
}

static ssize_t write_at(int fd, const void *buffer, size_t count, off_t offset)
{
    if (lseek(fd, offset, SEEK_SET) != offset)
        return -1;
    return write(fd, buffer, count);
}

/*
 * Overwriting a file in place: hello.txt changed through one fd and read back
 * through another, up to its end exactly, big.txt across the first
 * single-indirect block. All restored, sizes unchanged (growing is
 * test_grow's, on grow/user).
 */
static void test_writes(void)
{
    char buffer[64];
    static const char text[] = "Hello from the disk!\n";

    int fd = open(MNT "/hello.txt", O_RDWR);
    CHECK(fd >= 0);
    if (fd < 0)
        return;

    CHECK(write_at(fd, "HELLO", 5, 0) == 5);
    CHECK(lseek(fd, 0, SEEK_CUR) == 5);
    int other = open(MNT "/hello.txt", O_RDONLY);
    CHECK(other >= 0);
    memset(buffer, 0, sizeof(buffer));
    CHECK(read_full(other, buffer, sizeof(buffer)) == (ssize_t)strlen(text));
    CHECK(memcmp(buffer, "HELLO from the disk!\n", strlen(text)) == 0);
    close(other);

    // Up to the end exactly (nothing past it: hello.txt must keep its size); 0 bytes 0
    CHECK(write_at(fd, "DISK!\n", 6, 15) == 6);
    CHECK(lseek(fd, 0, SEEK_CUR) == (off_t)strlen(text));
    CHECK(write(fd, "x", 0) == 0);

    struct stat st;
    CHECK(fstat(fd, &st) == 0 && st.st_size == (off_t)strlen(text));
    CHECK(write_at(fd, text, strlen(text), 0) == (ssize_t)strlen(text));
    close(fd);
    expect_text(MNT "/hello.txt", text);

    // Write-only, in the middle of big.txt: across the end of the direct blocks
    fd = open(MNT "/big.txt", O_WRONLY);
    CHECK(fd >= 0);
    CHECK(write_at(fd, "9999999\n8888888\n", 16, 1535 * 8) == 16);
    errno = 0;
    CHECK(read(fd, buffer, 1) == -1 && errno == EBADF);
    close(fd);
    fd = open(MNT "/big.txt", O_RDONLY);
    CHECK(fd >= 0);
    CHECK(lseek(fd, 1534 * 8, SEEK_SET) == 1534 * 8 && read_full(fd, buffer, 32) == 32);
    CHECK(line_ok(buffer, 1534) && memcmp(buffer + 8, "9999999\n8888888\n", 16) == 0 && line_ok(buffer + 24, 1537));
    close(fd);
    fd = open(MNT "/big.txt", O_WRONLY);
    CHECK(fd >= 0);
    CHECK(write_at(fd, "0001535\n0001536\n", 16, 1535 * 8) == 16);
    close(fd);
}

/*
 * Growing a file (grow/user: empty on the test disk, no other test reads it)
 * through write(): one write() of 3000 bytes (several chunks and blocks), an
 * append at SEEK_END, and a write far past the end leaving a hole of zeros.
 */
static void test_grow(void)
{
    static char data[3000], back[10100];
    for (size_t i = 0; i < sizeof(data); i++)
        data[i] = (char)('a' + i % 26);

    int fd = open(MNT "/grow/user", O_RDWR);
    CHECK(fd >= 0);
    if (fd < 0)
        return;

    CHECK(write(fd, data, sizeof(data)) == (ssize_t)sizeof(data));
    struct stat st;
    CHECK(fstat(fd, &st) == 0 && st.st_size == (off_t)sizeof(data));

    CHECK(lseek(fd, 0, SEEK_END) == (off_t)sizeof(data));
    CHECK(write(fd, "end", 3) == 3);
    CHECK(fstat(fd, &st) == 0 && st.st_size == (off_t)sizeof(data) + 3);

    int other = open(MNT "/grow/user", O_RDONLY);
    CHECK(other >= 0);
    CHECK(read_full(other, back, sizeof(back)) == (ssize_t)sizeof(data) + 3);
    CHECK(memcmp(back, data, sizeof(data)) == 0 && memcmp(back + sizeof(data), "end", 3) == 0);

    // Far past the end: the file grows to it, the gap reads as zeros
    CHECK(write_at(fd, "far", 3, 10000) == 3);
    CHECK(fstat(fd, &st) == 0 && st.st_size == 10003);
    CHECK(lseek(other, sizeof(data) + 3, SEEK_SET) == (off_t)sizeof(data) + 3);
    CHECK(read_full(other, back, sizeof(back)) == 10003 - (ssize_t)sizeof(data) - 3);
    int zeros = 1;
    for (int i = 0; i < 10000 - (int)sizeof(data) - 3; i++)
        zeros = zeros && back[i] == 0;
    CHECK(zeros && memcmp(back + 10000 - sizeof(data) - 3, "far", 3) == 0);
    close(other);
    close(fd);
}

/*
 * Truncating (grow/user, 10003 bytes after test_grow): ftruncate down and up
 * (a hole), truncate by path, O_TRUNC (ignored without write access), and
 * what is refused: a read-only fd and a negative length (EINVAL), a directory
 * (EISDIR), a device (EINVAL), the initrd (EROFS), a missing path (ENOENT).
 */
static void test_truncate(void)
{
    static char back[5100];
    struct stat st;

    int fd = open(MNT "/grow/user", O_RDWR);
    CHECK(fd >= 0);
    if (fd < 0)
        return;

    CHECK(ftruncate(fd, 3003) == 0);
    CHECK(fstat(fd, &st) == 0 && st.st_size == 3003);
    CHECK(lseek(fd, 2990, SEEK_SET) == 2990 && read_full(fd, back, 100) == 13);
    CHECK(memcmp(back + 10, "end", 3) == 0);

    // Up: the new part reads as zeros; the offset never moves
    CHECK(lseek(fd, 7, SEEK_SET) == 7);
    CHECK(ftruncate(fd, 5000) == 0 && lseek(fd, 0, SEEK_CUR) == 7);
    CHECK(lseek(fd, 3003, SEEK_SET) == 3003 && read_full(fd, back, sizeof(back)) == 5000 - 3003);
    int zeros = 1;
    for (int i = 0; i < 5000 - 3003; i++)
        zeros = zeros && back[i] == 0;
    CHECK(zeros);

    CHECK(truncate(MNT "/grow/user", 100) == 0);
    CHECK(stat(MNT "/grow/user", &st) == 0 && st.st_size == 100);
    close(fd);

    // O_TRUNC: empties a file opened for writing, is ignored for one opened read-only
    fd = open(MNT "/grow/user", O_RDONLY | O_TRUNC);
    CHECK(fd >= 0 && fstat(fd, &st) == 0 && st.st_size == 100);
    errno = 0;
    CHECK(ftruncate(fd, 0) == -1 && errno == EINVAL); // not open for writing
    close(fd);
    fd = open(MNT "/grow/user", O_WRONLY | O_TRUNC);
    CHECK(fd >= 0 && fstat(fd, &st) == 0 && st.st_size == 0);
    errno = 0;
    CHECK(ftruncate(fd, -1) == -1 && errno == EINVAL);
    close(fd);

    errno = 0;
    CHECK(truncate(MNT "/docs", 0) == -1 && errno == EISDIR);
    errno = 0;
    CHECK(truncate("/dev/null", 0) == -1 && errno == EINVAL);
    errno = 0;
    CHECK(truncate("/usr/tests/ext2", 0) == -1 && errno == EROFS);
    errno = 0;
    CHECK(truncate(MNT "/nope", 0) == -1 && errno == ENOENT);
}

/*
 * Creating from user space (in grow/, after the kernel's creates): open with
 * O_CREAT | O_EXCL and a mode, stat of the new file, EEXIST for a second O_EXCL,
 * creat emptying an existing file without changing its mode, and the refusals:
 * EISDIR (a trailing '/'), ENOENT (a missing parent, the symlink's name), EROFS.
 */
static void test_create(void)
{
    struct stat st;
    int fd = open(MNT "/grow/made", O_WRONLY | O_CREAT | O_EXCL, 0600);
    CHECK(fd >= 0);
    CHECK(write(fd, "made in user space\n", 19) == 19);
    close(fd);
    CHECK(stat(MNT "/grow/made", &st) == 0 && S_ISREG(st.st_mode) && (st.st_mode & 07777) == 0600);
    CHECK(st.st_size == 19 && st.st_nlink == 1);
    expect_text(MNT "/grow/made", "made in user space\n");

    errno = 0;
    CHECK(open(MNT "/grow/made", O_RDWR | O_CREAT | O_EXCL, 0600) == -1 && errno == EEXIST);

    fd = creat(MNT "/grow/made", 0644);
    CHECK(fd >= 0 && fstat(fd, &st) == 0 && st.st_size == 0 && (st.st_mode & 07777) == 0600);
    close(fd);

    errno = 0;
    CHECK(open(MNT "/grow/made/", O_WRONLY | O_CREAT, 0644) == -1 && errno == ENOTDIR);
    errno = 0;
    CHECK(open(MNT "/grow/newdir/", O_WRONLY | O_CREAT, 0644) == -1 && errno == EISDIR);
    errno = 0;
    CHECK(open(MNT "/nope/made", O_WRONLY | O_CREAT, 0644) == -1 && errno == ENOENT);
    errno = 0;
    CHECK(open(MNT "/link", O_WRONLY | O_CREAT, 0644) == -1 && errno == ENOENT);
    errno = 0;
    CHECK(creat("/usr/tests/made", 0644) == -1 && errno == EROFS);
}

static void test_refusals(void)
{
    char buffer[64];

    errno = 0;
    CHECK(open(MNT "/link", O_RDONLY) == -1 && errno == ENOENT);
    errno = 0;
    CHECK(open(MNT "/hello.txt/", O_RDONLY) == -1 && errno == ENOTDIR);
    errno = 0;
    CHECK(open(MNT "/nope", O_RDONLY) == -1 && errno == ENOENT);
    errno = 0;
    CHECK(opendir(MNT "/hello.txt") == NULL && errno == ENOTDIR);

    int fd = open(MNT "/docs", O_RDONLY);
    CHECK(fd >= 0);
    errno = 0;
    CHECK(read(fd, buffer, sizeof(buffer)) == -1 && errno == EISDIR);
    close(fd);

    fd = open(MNT "/hello.txt", O_RDONLY);
    errno = 0;
    CHECK(write(fd, "x", 1) == -1 && errno == EBADF);
    errno = 0;
    CHECK(getdents(fd, buffer, sizeof(buffer)) == -1 && errno == ENOTDIR);
    close(fd);
}

int main(void)
{
    memset(long_name, 'a', LONG_NAME_LENGTH);
    memcpy(long_path, MNT "/", sizeof(MNT));
    memcpy(long_path + sizeof(MNT), long_name, LONG_NAME_LENGTH + 1);

    test_listing();
    test_stat();
    test_reads();
    test_writes();
    test_grow();
    test_truncate();
    test_create();
    test_refusals();

    // Every fd and stream was closed: the next open gets fd 3 again
    int fd = open("/", O_RDONLY);
    CHECK(fd == 3);
    close(fd);

    return test_status();
}
