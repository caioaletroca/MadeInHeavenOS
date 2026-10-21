#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <test.h>

/*
 * Directories: opendir/readdir/rewinddir/closedir, and the getdents records
 * under them. The initrd's layout is known: / holds bin, dev, mnt, sbin and
 * usr (in archive order), /usr/tests holds every test, this one included.
 */

#define TESTS "/usr/tests"
#define ROOT_COUNT 7
#define ROOT_RECORDS 112 // ".", "..", "bin", "dev", "mnt", "sbin", "usr": 16 bytes each

// One directory's listing, as readdir returns it
typedef struct listing
{
    int count;
    int hello, run, dirs; // times each was seen
    int dots_first;       // "." then ".." as the first two entries
    ino_t self, parent;   // inode numbers of "." and ".."
    int types_ok;         // every entry had DT_DIR or DT_REG, as expected
    int end_errno;        // errno after the NULL at the end (0: the end, not an error)
} listing_t;

static void list(DIR *dir, listing_t *out)
{
    memset(out, 0, sizeof(*out));
    out->types_ok = 1;

    errno = 0;
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL)
    {
        const char *name = entry->d_name;

        if (out->count == 0)
        {
            out->dots_first = strcmp(name, ".") == 0;
            out->self = entry->d_ino;
        }
        else if (out->count == 1)
        {
            out->dots_first = out->dots_first && strcmp(name, "..") == 0;
            out->parent = entry->d_ino;
        }

        int is_dot = strcmp(name, ".") == 0 || strcmp(name, "..") == 0;
        if (entry->d_type != (is_dot ? DT_DIR : DT_REG)) // /usr/tests holds only programs
            out->types_ok = 0;

        out->hello += strcmp(name, "hello") == 0;
        out->run += strcmp(name, "run") == 0;
        out->dirs += strcmp(name, "dirs") == 0;
        out->count++;
    }
    out->end_errno = errno;
}

// The inode number of `name` in directory `path`, or 0
static ino_t ino_of(const char *path, const char *name)
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

static void test_readdir(void)
{
    DIR *dir = opendir(TESTS);
    CHECK(dir != NULL);
    if (dir == NULL)
        return;

    listing_t first;
    list(dir, &first);
    CHECK(first.dots_first);
    CHECK(first.types_ok);
    CHECK(first.hello == 1 && first.run == 1 && first.dirs == 1);
    CHECK(first.count >= 5);
    CHECK(first.end_errno == 0);

    // Past the end: NULL again, still not an error
    errno = 0;
    CHECK(readdir(dir) == NULL);
    CHECK(errno == 0);

    // rewinddir starts over and gives the same listing
    rewinddir(dir);
    listing_t second;
    list(dir, &second);
    CHECK(second.count == first.count);
    CHECK(second.dots_first && second.self == first.self && second.parent == first.parent);

    CHECK(closedir(dir) == 0);

    // Inode numbers agree from both sides: "." here is "tests" in /usr, ".." is "." of /usr
    CHECK(first.self != 0 && first.self == ino_of("/usr", "tests"));
    CHECK(first.parent != 0 && first.parent == ino_of("/usr", "."));
    CHECK(ino_of("/", ".") != 0 && ino_of("/", ".") == ino_of("/", "..")); // the root is its own parent
    CHECK(ino_of("/", "usr") == ino_of("/usr", "."));
}

static void test_opendir_errors(void)
{
    errno = 0;
    CHECK(opendir("/sbin/init") == NULL && errno == ENOTDIR);
    errno = 0;
    CHECK(opendir("/nope") == NULL && errno == ENOENT);
    errno = 0;
    CHECK(opendir("usr") == NULL && errno == ENOENT); // relative: no current directory yet
    errno = 0;
    CHECK(opendir("/usr/tests/hello/") == NULL && errno == ENOTDIR);

    // O_DIRECTORY only restricts: open without it still takes a directory, and a file
    int fd = open("/usr", O_RDONLY);
    CHECK(fd >= 0);
    close(fd);
    fd = open("/sbin/init", O_RDONLY);
    CHECK(fd >= 0);
    close(fd);
}

// Names of the records in buffer[0..n), checked against `expected`; returns how many matched
static int records_match(const void *buffer, long n, const char *const expected[], int count)
{
    const unsigned char *bytes = buffer;
    int matched = 0;
    long used = 0;

    while (used < n && matched < count)
    {
        const struct mihos_dirent *record = (const void *)(bytes + used);
        if (record->d_reclen == 0 || record->d_reclen % 8 != 0 || strcmp(record->d_name, expected[matched]) != 0)
            break;
        used += record->d_reclen;
        matched++;
    }

    return used == n ? matched : -1;
}

static void test_getdents(void)
{
    static const char *const root[] = {".", "..", "bin", "dev", "mnt", "sbin", "usr"};
    uint64_t buffer[64]; // 512 bytes, 8-aligned like the records

    int fd = open("/", O_RDONLY | O_DIRECTORY);
    CHECK(fd >= 0);

    // Too small for one record
    errno = 0;
    CHECK(getdents(fd, buffer, 8) == -1 && errno == EINVAL);

    // Exactly one record, then the rest, then the end (again and again)
    CHECK(getdents(fd, buffer, 16) == 16);
    CHECK(records_match(buffer, 16, root, 1) == 1);
    long n = getdents(fd, buffer, sizeof(buffer));
    CHECK(n == ROOT_RECORDS - 16);
    CHECK(records_match(buffer, n, root + 1, ROOT_COUNT - 1) == ROOT_COUNT - 1);
    CHECK(getdents(fd, buffer, sizeof(buffer)) == 0);
    CHECK(getdents(fd, buffer, sizeof(buffer)) == 0);

    // lseek to 0 rewinds; a bad pointer fails and loses nothing
    CHECK(lseek(fd, 0, SEEK_SET) == 0);
    errno = 0;
    CHECK(getdents(fd, (void *)8, sizeof(buffer)) == -1 && errno == EFAULT);
    n = getdents(fd, buffer, sizeof(buffer));
    CHECK(n == ROOT_RECORDS);
    CHECK(records_match(buffer, n, root, ROOT_COUNT) == ROOT_COUNT);

    // The padding after each name is zero (no kernel memory leaks out)
    const unsigned char *bytes = (const unsigned char *)buffer;
    int padding_clean = 1;
    for (long used = 0; used < n;)
    {
        const struct mihos_dirent *record = (const void *)(bytes + used);
        size_t end = offsetof(struct mihos_dirent, d_name) + strlen(record->d_name) + 1;
        for (size_t i = end; i < record->d_reclen; i++)
            padding_clean = padding_clean && bytes[used + i] == 0;
        used += record->d_reclen;
    }
    CHECK(padding_clean);

    CHECK(close(fd) == 0);

    // Small buffers: the kernel resumes where it stopped, and the total matches readdir
    fd = open(TESTS, O_RDONLY | O_DIRECTORY);
    int records = 0;
    for (;;)
    {
        n = getdents(fd, buffer, 40); // one or two records per call
        if (n <= 0)
            break;
        for (long used = 0; used < n; records++)
            used += ((const struct mihos_dirent *)((const unsigned char *)buffer + used))->d_reclen;
    }
    CHECK(n == 0);
    close(fd);

    DIR *dir = opendir(TESTS);
    int entries = 0;
    while (readdir(dir) != NULL)
        entries++;
    closedir(dir);
    CHECK(records == entries);

    // Not a directory, not open
    errno = 0;
    CHECK(getdents(0, buffer, sizeof(buffer)) == -1 && errno == ENOTDIR); // the console
    fd = open("/sbin/init", O_RDONLY);
    errno = 0;
    CHECK(getdents(fd, buffer, sizeof(buffer)) == -1 && errno == ENOTDIR);
    close(fd);
    errno = 0;
    CHECK(getdents(fd, buffer, sizeof(buffer)) == -1 && errno == EBADF);
    errno = 0;
    CHECK(getdents(99, buffer, sizeof(buffer)) == -1 && errno == EBADF);
}

// /dev is another file system mounted on the initrd's dev/: listing it lists devfs
static void test_mounted(void)
{
    DIR *dir = opendir("/dev");
    CHECK(dir != NULL);
    if (dir == NULL)
        return;

    int dots = 0;
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL)
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            dots++;
    CHECK(dots == 2);
    closedir(dir);

    // Lookups step out of the mount through ".."
    dir = opendir("/dev/../usr/tests");
    CHECK(dir != NULL);
    if (dir != NULL)
        closedir(dir);
}

int main(void)
{
    test_readdir();
    test_opendir_errors();
    test_getdents();
    test_mounted();

    // Every stream and fd was closed: the next open gets fd 3 again
    int fd = open("/", O_RDONLY);
    CHECK(fd == 3);
    close(fd);

    return test_status();
}
