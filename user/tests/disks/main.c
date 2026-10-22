#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <sys/ioctl.h>
#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <test.h>

/*
 * Block devices in /dev: the test disk (disk.img, 4 MiB of ext2 labelled
 * "mihos-test") found by its d_type, what stat says about it, and reads and
 * writes at byte offsets through open, lseek, read and write. hda on QEMU, hdb
 * on Bochs.
 */

#define DISK_SIZE (4 * 1024 * 1024)
#define DISK_LABEL "mihos-test"
#define SB_MAGIC 1080       // the superblock is at 1024, its magic at 56 in it
#define SB_VOLUME_NAME 1144 // and the label at 120
#define EXT2_MAGIC 0xEF53
#define WRITE_AREA (7000 * 512 + 100) // free sectors; 100 bytes in, so writes start mid-sector
#define WRITE_LENGTH 5000             // across sectors and the 4 KiB cache block boundary at 7008

static char disk[64];

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

static ssize_t read_at(int fd, void *buffer, size_t count, off_t offset)
{
    if (lseek(fd, offset, SEEK_SET) != offset)
        return -1;
    return read_full(fd, buffer, count);
}

// The first DT_BLK entry in /dev, as a path in disk
static int find_disk(void)
{
    DIR *dir = opendir("/dev");
    CHECK(dir != NULL);
    if (dir == NULL)
        return 0;

    int found = 0;
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL)
    {
        if (entry->d_type != DT_BLK)
            continue;

        if (!found)
        {
            strcpy(disk, "/dev/");
            strlcpy(disk + 5, entry->d_name, sizeof(disk) - 5);
            struct stat st;
            CHECK(stat(disk, &st) == 0 && st.st_ino == entry->d_ino);
        }
        found++;
    }
    closedir(dir);

    CHECK(found >= 1);
    return found >= 1;
}

static void test_stat(void)
{
    struct stat st;
    CHECK(stat(disk, &st) == 0);
    CHECK(S_ISBLK(st.st_mode) && !S_ISCHR(st.st_mode) && !S_ISREG(st.st_mode));
    CHECK((st.st_mode & 07777) == 0660);
    CHECK(major(st.st_rdev) == 3 || major(st.st_rdev) == 22); // IDE, Linux's numbers
    CHECK(minor(st.st_rdev) == 0 || minor(st.st_rdev) == 64); // master or slave
    CHECK(st.st_size == 0 && st.st_blocks == 0);              // devices report no size (Linux)

    struct stat null;
    CHECK(stat("/dev/null", &null) == 0 && st.st_dev == null.st_dev); // on devfs

    int fd = open(disk, O_RDONLY);
    CHECK(fd >= 0);
    struct stat opened;
    memset(&opened, 0, sizeof(opened));
    memset(&st, 0, sizeof(st));
    CHECK(stat(disk, &st) == 0 && fstat(fd, &opened) == 0);
    CHECK(memcmp(&st, &opened, sizeof(st)) == 0);
    close(fd);
}

static void test_reads(void)
{
    int fd = open(disk, O_RDONLY);
    CHECK(fd >= 0);
    if (fd < 0)
        return;

    // The size: lseek to the end, not stat
    CHECK(lseek(fd, 0, SEEK_END) == DISK_SIZE);
    CHECK(lseek(fd, 0, SEEK_CUR) == DISK_SIZE);

    // The superblock's magic and label, at byte offsets inside sector 2
    unsigned char magic[2];
    CHECK(read_at(fd, magic, 2, SB_MAGIC) == 2);
    CHECK((magic[0] | magic[1] << 8) == EXT2_MAGIC);
    char label[sizeof(DISK_LABEL)];
    CHECK(read_at(fd, label, sizeof(label), SB_VOLUME_NAME) == (ssize_t)sizeof(label));
    CHECK(memcmp(label, DISK_LABEL, sizeof(label)) == 0);

    // Reads go on where the last one stopped: sectors 1 and 2 at once equal the two halves
    static char whole[1024], halves[1024];
    CHECK(read_at(fd, whole, sizeof(whole), 512) == (ssize_t)sizeof(whole));
    CHECK(read_at(fd, halves, 300, 512) == 300);
    CHECK(read_full(fd, halves + 300, sizeof(halves) - 300) == (ssize_t)sizeof(halves) - 300);
    CHECK(memcmp(whole, halves, sizeof(whole)) == 0);
    CHECK(lseek(fd, 0, SEEK_CUR) == 512 + (off_t)sizeof(whole));

    // Across the end: only what is left, then 0
    char tail[100];
    memset(tail, 'x', sizeof(tail));
    CHECK(read_at(fd, tail, sizeof(tail), DISK_SIZE - 10) == 10);
    CHECK(tail[10] == 'x');
    CHECK(read(fd, tail, sizeof(tail)) == 0);
    CHECK(read_at(fd, tail, sizeof(tail), DISK_SIZE + 1000) == 0); // seeking past the end is allowed

    close(fd);
}

static ssize_t write_at(int fd, const void *buffer, size_t count, off_t offset)
{
    if (lseek(fd, offset, SEEK_SET) != offset)
        return -1;
    return write(fd, buffer, count);
}

/*
 * Writes at byte offsets in the free sectors 7000.. (ext2 blocks 3500.., which
 * nothing reads) and at the end of the disk, all restored. One write() of
 * several 256-byte chunks, read back through the same fd and another one.
 */
static void test_writes(void)
{
    static char saved[WRITE_LENGTH], pattern[WRITE_LENGTH], back[WRITE_LENGTH];
    char tail[10];

    int fd = open(disk, O_RDWR);
    CHECK(fd >= 0);
    if (fd < 0)
        return;
    CHECK(read_at(fd, saved, sizeof(saved), WRITE_AREA) == (ssize_t)sizeof(saved));
    CHECK(read_at(fd, tail, sizeof(tail), DISK_SIZE - 10) == 10);

    // Across sectors and a 4 KiB cache block; the position moves past what was written
    for (size_t i = 0; i < sizeof(pattern); i++)
        pattern[i] = (char)(i * 13 + 5);
    CHECK(write_at(fd, pattern, sizeof(pattern), WRITE_AREA) == (ssize_t)sizeof(pattern));
    CHECK(lseek(fd, 0, SEEK_CUR) == WRITE_AREA + (off_t)sizeof(pattern));
    CHECK(read_at(fd, back, sizeof(back), WRITE_AREA) == (ssize_t)sizeof(back));
    CHECK(memcmp(back, pattern, sizeof(back)) == 0);

    // Another open file of the disk sees the same bytes (one cache)
    int other = open(disk, O_RDONLY);
    CHECK(other >= 0);
    memset(back, 0, sizeof(back));
    CHECK(read_at(other, back, sizeof(back), WRITE_AREA) == (ssize_t)sizeof(back));
    CHECK(memcmp(back, pattern, sizeof(back)) == 0);
    close(other);

    // Write-only: writes work, reads are refused
    int writer = open(disk, O_WRONLY);
    CHECK(writer >= 0);
    CHECK(write_at(writer, "abc", 3, WRITE_AREA + 1) == 3);
    errno = 0;
    CHECK(read(writer, back, 1) == -1 && errno == EBADF);
    close(writer);
    CHECK(read_at(fd, back, 5, WRITE_AREA) == 5);
    CHECK(back[0] == pattern[0] && memcmp(back + 1, "abc", 3) == 0 && back[4] == pattern[4]);

    // The end: a write across it is cut short, at or past it ENOSPC, 0 bytes is 0
    CHECK(write_at(fd, "0123456789ABCDEF", 16, DISK_SIZE - 10) == 10);
    CHECK(read_at(fd, back, 16, DISK_SIZE - 10) == 10 && memcmp(back, "0123456789", 10) == 0);
    errno = 0;
    CHECK(write_at(fd, "x", 1, DISK_SIZE) == -1 && errno == ENOSPC);
    errno = 0;
    CHECK(write_at(fd, "x", 1, DISK_SIZE + 1000) == -1 && errno == ENOSPC);
    CHECK(write_at(fd, "x", 0, DISK_SIZE) == 0);

    // Put everything back
    CHECK(write_at(fd, tail, sizeof(tail), DISK_SIZE - 10) == 10);
    CHECK(write_at(fd, saved, sizeof(saved), WRITE_AREA) == (ssize_t)sizeof(saved));
    CHECK(read_at(fd, back, sizeof(back), WRITE_AREA) == (ssize_t)sizeof(back));
    CHECK(memcmp(back, saved, sizeof(back)) == 0);
    close(fd);
}

static void test_refusals(void)
{
    errno = 0;
    CHECK(open(disk, O_ACCMODE) == -1 && errno == EINVAL);
    errno = 0;
    CHECK(open("/dev", O_WRONLY) == -1 && errno == EISDIR);
    errno = 0;
    CHECK(open(disk, O_RDONLY | O_DIRECTORY) == -1 && errno == ENOTDIR);

    int fd = open(disk, O_RDONLY);
    CHECK(fd >= 0);
    errno = 0;
    CHECK(write(fd, "x", 1) == -1 && errno == EBADF);
    struct winsize ws;
    errno = 0;
    CHECK(ioctl(fd, TIOCGWINSZ, &ws) == -1 && errno == ENOTTY);
    CHECK(isatty(fd) == 0);
    char buffer[64];
    errno = 0;
    CHECK(getdents(fd, buffer, sizeof(buffer)) == -1 && errno == ENOTDIR);
    close(fd);
}

int main(void)
{
    if (find_disk())
    {
        test_stat();
        test_reads();
        test_writes();
        test_refusals();
    }

    // Every fd was closed: the next open gets fd 3 again
    int fd = open("/dev/null", O_RDONLY);
    CHECK(fd == 3);
    close(fd);

    return test_status();
}
