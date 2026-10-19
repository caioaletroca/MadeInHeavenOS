#include <selftest/vfs.h>
#include <fs/vfs.h>
#include <fs/file.h>
#include <mm/kmalloc.h>
#include <mihos/errno.h>
#include <mihos/fcntl.h>
#include <mihos/dirent.h>
#include <mihos/stat.h>
#include <mihos/ioctl.h>
#include <driver/console.h>
#include <fs/devfs.h>
#include <util.h>
#include <string.h>
#include <panic.h>

#define ELF_MAGIC "\x7F" "ELF"
#define HELLO "/usr/tests/hello"

/**
 * Looks up a path that must exist with the given type; returns the reference.
 */
static vfs_node_t *vfs_expect(const char *path, vfs_node_type_t type)
{
    vfs_node_t *node = NULL;
    int ret = vfs_lookup(path, &node);

    if (ret != 0)
        panic("VFS self-test: lookup of '%s' failed: %d\n", path, ret);
    if (node->type != type)
        panic("VFS self-test: '%s' has the wrong type\n", path);

    return node;
}

/**
 * Looks up a path that must fail with error, without returning a node.
 */
static void vfs_expect_error(const char *path, int error)
{
    vfs_node_t *node = NULL;
    int ret = vfs_lookup(path, &node);

    if (ret != error)
        panic("VFS self-test: '%s' returned %d instead of %d\n", path, ret, error);
    if (node != NULL)
        panic("VFS self-test: a failed lookup of '%s' set the node\n", path);
}

/**
 * Two paths must lead to the very same node (one node per file).
 */
static void vfs_expect_same(const char *a, const char *b, vfs_node_type_t type)
{
    vfs_node_t *x = vfs_expect(a, type);
    vfs_node_t *y = vfs_expect(b, type);

    if (x != y)
        panic("VFS self-test: '%s' and '%s' are different nodes\n", a, b);

    vfs_node_put(x);
    vfs_node_put(y);
}

static void vfs_test_lookup(void)
{
    vfs_node_put(vfs_expect("/", VFS_DIRECTORY));
    vfs_node_put(vfs_expect("/usr", VFS_DIRECTORY));
    vfs_node_put(vfs_expect("/usr/tests", VFS_DIRECTORY));
    vfs_node_put(vfs_expect("/usr/tests/", VFS_DIRECTORY)); // a trailing '/' on a directory is fine
    vfs_node_put(vfs_expect("/sbin/init", VFS_FILE));
    vfs_node_put(vfs_expect("/bin/sh", VFS_FILE));

    // Repeated slashes, "." and ".." reach the same node
    vfs_expect_same(HELLO, "//usr///tests/hello", VFS_FILE);
    vfs_expect_same(HELLO, "/./usr/./tests/./hello", VFS_FILE);
    vfs_expect_same(HELLO, "/usr/tests/../tests/hello", VFS_FILE);
    vfs_expect_same(HELLO, "/bin/../usr/tests/hello", VFS_FILE);
    vfs_expect_same("/", "/..", VFS_DIRECTORY); // the root is its own parent
    vfs_expect_same("/", "/usr/..", VFS_DIRECTORY);

    vfs_expect_error("/nope", -ENOENT);
    vfs_expect_error("/usr/nope/hello", -ENOENT);
    vfs_expect_error("usr/tests/hello", -ENOENT); // relative: no current directory yet
    vfs_expect_error("", -ENOENT);
    vfs_expect_error("/usr/tests/hell", -ENOENT);      // a prefix of a name
    vfs_expect_error("/usr/tests/hello.elf", -ENOENT); // the suffix programs lost
    vfs_expect_error("/usr/tests/hello/", -ENOTDIR);   // a trailing '/' promises a directory
    vfs_expect_error("/sbin/init/x", -ENOTDIR);
    vfs_expect_error("/sbin/init/.", -ENOTDIR);

    // A component of VFS_NAME_MAX characters is allowed (just missing), one more is not
    char path[VFS_NAME_MAX + 3];
    path[0] = '/';
    memset(path + 1, 'a', VFS_NAME_MAX);
    path[VFS_NAME_MAX + 1] = '\0';
    vfs_expect_error(path, -ENOENT);
    path[VFS_NAME_MAX + 1] = 'a';
    path[VFS_NAME_MAX + 2] = '\0';
    vfs_expect_error(path, -ENAMETOOLONG);
}

static void vfs_test_read(void)
{
    void *data;
    size_t size;
    int ret = vfs_read_file(HELLO, &data, &size);
    if (ret != 0)
        panic("VFS self-test: reading %s failed: %d\n", HELLO, ret);
    if (size < 64 || memcmp(data, ELF_MAGIC, 4) != 0)
        panic("VFS self-test: %s is not an ELF image (%zu bytes)\n", HELLO, size);

    vfs_node_t *hello = vfs_expect(HELLO, VFS_FILE);
    if (hello->size != size)
        panic("VFS self-test: node size %u, read %zu bytes\n", (unsigned int)hello->size, size);

    unsigned char buffer[16];

    // From the start, and from the middle: the same bytes as the whole-file read
    if (vfs_read(hello, buffer, sizeof(buffer), 0) != sizeof(buffer) || memcmp(buffer, data, sizeof(buffer)) != 0)
        panic("VFS self-test: read at offset 0 differs\n");
    if (vfs_read(hello, buffer, sizeof(buffer), 100) != sizeof(buffer) ||
        memcmp(buffer, (unsigned char *)data + 100, sizeof(buffer)) != 0)
        panic("VFS self-test: read at offset 100 differs\n");

    // Across the end: only what is left; at or past the end: 0
    if (vfs_read(hello, buffer, sizeof(buffer), size - 4) != 4 ||
        memcmp(buffer, (unsigned char *)data + size - 4, 4) != 0)
        panic("VFS self-test: read across the end is wrong\n");
    if (vfs_read(hello, buffer, sizeof(buffer), size) != 0 || vfs_read(hello, buffer, sizeof(buffer), size + 100) != 0)
        panic("VFS self-test: read at the end did not return 0\n");

    vfs_node_put(hello);
    kfree(data);

    vfs_node_t *dir = vfs_expect("/usr/tests", VFS_DIRECTORY);
    if (vfs_read(dir, buffer, sizeof(buffer), 0) != -EISDIR)
        panic("VFS self-test: reading a directory did not return -EISDIR\n");
    vfs_node_put(dir);

    if (vfs_read_file("/usr/tests", &data, &size) != -EISDIR)
        panic("VFS self-test: vfs_read_file of a directory did not return -EISDIR\n");
    if (vfs_read_file("/nope", &data, &size) != -ENOENT)
        panic("VFS self-test: vfs_read_file of a missing file did not return -ENOENT\n");
}

/**
 * Reads entry `index` of dir, which must be `name` with the given type and inode number.
 */
static void vfs_expect_entry(vfs_node_t *dir, uint64_t index, const char *name, vfs_node_type_t type, uint64_t ino)
{
    vfs_dirent_t entry;
    entry.length = (size_t)-1; // poison: readdir must set it

    int ret = vfs_readdir(dir, index, &entry);
    if (ret != 1)
        panic("VFS self-test: readdir %u returned %d instead of '%s'\n", (unsigned int)index, ret, name);
    if (entry.length != strlen(name) || strcmp(entry.name, name) != 0)
        panic("VFS self-test: readdir %u gave '%s' (length %zu) instead of '%s'\n", (unsigned int)index, entry.name,
              entry.length, name);
    if (entry.type != type)
        panic("VFS self-test: '%s' has the wrong type in readdir\n", name);
    if (ino != 0 && entry.ino != ino)
        panic("VFS self-test: '%s' has inode %u in readdir, %u by lookup\n", name, (unsigned int)entry.ino,
              (unsigned int)ino);
}

static void vfs_test_readdir(void)
{
    vfs_dirent_t entry;
    vfs_node_t *root = vfs_expect("/", VFS_DIRECTORY);
    vfs_node_t *usr = vfs_expect("/usr", VFS_DIRECTORY);
    vfs_node_t *tests = vfs_expect("/usr/tests", VFS_DIRECTORY);
    vfs_node_t *hello = vfs_expect(HELLO, VFS_FILE);

    // The root: "." and ".." are both itself, then the archive's directories in order.
    // "dev" is the initrd's own directory (readdir does not follow mounts): no inode to compare
    vfs_expect_entry(root, 0, ".", VFS_DIRECTORY, root->ino);
    vfs_expect_entry(root, 1, "..", VFS_DIRECTORY, root->ino);
    vfs_expect_entry(root, 2, "bin", VFS_DIRECTORY, 0);
    vfs_expect_entry(root, 3, "dev", VFS_DIRECTORY, 0);
    vfs_expect_entry(root, 4, "sbin", VFS_DIRECTORY, 0);
    vfs_expect_entry(root, 5, "usr", VFS_DIRECTORY, usr->ino);
    if (vfs_readdir(root, 6, &entry) != 0 || vfs_readdir(root, 100, &entry) != 0)
        panic("VFS self-test: readdir past the root's last entry did not return 0\n");

    vfs_expect_entry(usr, 0, ".", VFS_DIRECTORY, usr->ino);
    vfs_expect_entry(usr, 1, "..", VFS_DIRECTORY, root->ino);
    vfs_expect_entry(usr, 2, "tests", VFS_DIRECTORY, tests->ino);

    // hello is somewhere in /usr/tests, with the inode number lookup gives
    int found = 0;
    for (uint64_t i = 0; vfs_readdir(tests, i, &entry) == 1; i++)
    {
        if (strcmp(entry.name, "hello") == 0)
        {
            found++;
            vfs_expect_entry(tests, i, "hello", VFS_FILE, hello->ino);
        }
    }
    if (found != 1)
        panic("VFS self-test: hello listed %d times in /usr/tests\n", found);

    if (vfs_readdir(hello, 0, &entry) != -ENOTDIR)
        panic("VFS self-test: readdir of a file did not return -ENOTDIR\n");

    vfs_node_put(hello);
    vfs_node_put(tests);
    vfs_node_put(usr);
    vfs_node_put(root);
}

// Every record of the root, as vfs_file_getdents packs it
static const char *const root_names[] = {".", "..", "bin", "dev", "sbin", "usr"};
#define ROOT_COUNT 6
#define ROOT_RECORDS 96 // 6 records of 16 bytes: 11-byte header + name + '\0', rounded to 8
#define SBIN_INDEX 4

static void vfs_test_getdents(void)
{
    struct file *file;
    if (vfs_open("/sbin/init", O_RDONLY | O_DIRECTORY, &file) != -ENOTDIR)
        panic("VFS self-test: O_DIRECTORY on a file did not return -ENOTDIR\n");
    if (vfs_open("/", O_RDONLY | O_DIRECTORY, &file) != 0)
        panic("VFS self-test: opening / with O_DIRECTORY failed\n");

    // Zeroed, like sys_getdents does; the byte after `size` is a canary
    unsigned char buffer[ROOT_RECORDS + 16];
    uint64_t next;
    long n;

    scoped_guard(mutex, &file->position_lock)
    {
        // The whole root at once
        memset(buffer, 0, sizeof(buffer));
        n = vfs_file_getdents(file, buffer, sizeof(buffer), &next);
        if (n != ROOT_RECORDS || next != ROOT_COUNT)
            panic("VFS self-test: getdents of / returned %ld, next %u\n", n, (unsigned int)next);

        size_t used = 0;
        for (size_t i = 0; i < ROOT_COUNT; i++)
        {
            const struct mihos_dirent *record = (const void *)(buffer + used);
            if (record->d_reclen != 16 || strcmp(record->d_name, root_names[i]) != 0 ||
                record->d_type != MIHOS_DT_DIR)
                panic("VFS self-test: getdents record %zu is wrong ('%s', %u bytes)\n", i, record->d_name,
                      (unsigned int)record->d_reclen);
            used += record->d_reclen;
        }

        // At the end: 0, and next stays
        file->offset = next;
        if (vfs_file_getdents(file, buffer, sizeof(buffer), &next) != 0 || next != ROOT_COUNT)
            panic("VFS self-test: getdents at the end did not return 0\n");

        // "sbin" fills its 16 bytes exactly: nothing may be written after them
        file->offset = SBIN_INDEX;
        memset(buffer, 0, sizeof(buffer));
        buffer[16] = 0xA5;
        n = vfs_file_getdents(file, buffer, 16, &next);
        if (n != 16 || next != SBIN_INDEX + 1 || buffer[16] != 0xA5)
            panic("VFS self-test: getdents into an exact fit returned %ld, next %u, canary 0x%x\n", n,
                  (unsigned int)next, buffer[16]);

        // Not even one record fits
        file->offset = 0;
        if (vfs_file_getdents(file, buffer, 15, &next) != -EINVAL)
            panic("VFS self-test: getdents into 15 bytes did not return -EINVAL\n");
    }

    file_put(file);
}

// A plausible build time: after 2023, before 2100. A garbage mtime fails this
#define MTIME_MIN 1700000000
#define MTIME_MAX 4102444800

/**
 * Stats a path that must exist with the given st_mode; returns its mtime.
 */
static int64_t vfs_expect_stat(const char *path, uint32_t mode)
{
    vfs_node_t *node = vfs_expect(path, (mode & MIHOS_S_IFMT) == MIHOS_S_IFDIR ? VFS_DIRECTORY : VFS_FILE);

    struct mihos_stat st;
    memset(&st, 0xA5, sizeof(st)); // every byte must be written, padding included
    vfs_stat(node, &st);

    if (st.st_mode != mode)
        panic("VFS self-test: stat of '%s' gave mode 0%o, expected 0%o\n", path, st.st_mode, mode);
    if (st.st_ino != node->ino || (uint64_t)st.st_size != node->size || st.st_nlink != 1)
        panic("VFS self-test: stat of '%s' disagrees with its node\n", path);
    if (st.st_blocks != (int64_t)(ALIGN_UP(node->size, 512) / 512))
        panic("VFS self-test: stat of '%s' has %u blocks\n", path, (unsigned int)st.st_blocks);
    if (st.st_dev == 0 || st.st_dev != node->dev)
        panic("VFS self-test: stat of '%s' has st_dev %u, its file system is %u\n", path, (unsigned int)st.st_dev,
              (unsigned int)node->dev);
    if (st.st_uid != 0 || st.st_gid != 0 || st.__pad0 != 0 || st.st_rdev != 0 || st.st_mtim.tv_nsec != 0)
        panic("VFS self-test: stat of '%s' left fields unset\n", path);
    if (st.st_atim.tv_sec != st.st_mtim.tv_sec || st.st_ctim.tv_sec != st.st_mtim.tv_sec)
        panic("VFS self-test: stat of '%s' has differing times\n", path);

    vfs_node_put(node);
    return st.st_mtim.tv_sec;
}

static void vfs_test_stat(void)
{
    // Files and listed directories carry the tar header's permissions and time
    int64_t hello = vfs_expect_stat(HELLO, MIHOS_S_IFREG | 0755);
    int64_t tests = vfs_expect_stat("/usr/tests", MIHOS_S_IFDIR | 0755);
    if (hello < MTIME_MIN || hello > MTIME_MAX || tests < MTIME_MIN || tests > MTIME_MAX)
        panic("VFS self-test: implausible mtime (hello %ld, /usr/tests %ld)\n", (long)hello, (long)tests);

    // The archive never lists "usr/" or the root: defaults 0755 and no time. A file's
    // values must not leak onto the directories its path passes through
    if (vfs_expect_stat("/usr", MIHOS_S_IFDIR | 0755) != 0)
        panic("VFS self-test: /usr took the mtime of an entry below it\n");
    if (vfs_expect_stat("/", MIHOS_S_IFDIR | 0755) != 0)
        panic("VFS self-test: the root has an mtime\n");

    // An open file is its lookup's node: what fstat reports is that node's stat
    struct file *file;
    if (vfs_open(HELLO, O_RDONLY, &file) != 0)
        panic("VFS self-test: opening %s failed\n", HELLO);
    vfs_node_t *node = vfs_expect(HELLO, VFS_FILE);
    if (file->node != node)
        panic("VFS self-test: an open %s is not its lookup's node\n", HELLO);
    vfs_node_put(node);
    file_put(file);

    // The console's open file is /dev/console: a devfs node, 5:1, readable and writable by root
    vfs_node_t *console = vfs_expect("/dev/console", VFS_CHARDEV);
    struct mihos_stat st;
    memset(&st, 0xA5, sizeof(st));
    vfs_stat(console, &st);
    if (st.st_mode != (MIHOS_S_IFCHR | 0600) || st.st_rdev != MIHOS_MKDEV(5, 1) || st.st_size != 0 ||
        st.st_dev != console->dev || st.__pad0 != 0)
        panic("VFS self-test: /dev/console is wrong (mode 0%o)\n", st.st_mode);

    file = console_file();
    if (file->node != console || file->ops != console->device_ops ||
        file->flags != (FILE_READ | FILE_WRITE))
        panic("VFS self-test: console_file() is not /dev/console opened read-write\n");

    // The request encodes what sys_ioctl copies: TIOCGWINSZ reads 8 bytes out
    if (MIHOS_IOC_DIR(MIHOS_TIOCGWINSZ) != MIHOS_IOC_READ ||
        MIHOS_IOC_SIZE(MIHOS_TIOCGWINSZ) != sizeof(struct mihos_winsize))
        panic("VFS self-test: TIOCGWINSZ does not encode a read of a winsize\n");

    // The console answers the window size (into a kernel buffer) and refuses the rest
    struct mihos_winsize ws;
    memset(&ws, 0, sizeof(ws));
    if (file->ops->ioctl == NULL || file->ops->ioctl(file, MIHOS_TIOCGWINSZ, &ws) != 0 || ws.ws_row != 25 ||
        ws.ws_col != 80 || ws.ws_xpixel != 0 || ws.ws_ypixel != 0)
        panic("VFS self-test: the console's TIOCGWINSZ gave %ux%u\n", ws.ws_row, ws.ws_col);
    if (file->ops->ioctl(file, MIHOS_IOR('T', 0xFF, int), &ws) != -ENOTTY)
        panic("VFS self-test: the console accepted an unknown request\n");
    file_put(file);
    vfs_node_put(console);
}

static void vfs_test_mount(void)
{
    vfs_node_t *root = vfs_expect("/", VFS_DIRECTORY);
    vfs_node_t *dev = vfs_expect("/dev", VFS_DIRECTORY);
    unsigned int dev_refs = dev->refs;

    // Lookup crossed into devfs: its own root, another file system
    if (dev->covers == NULL || dev->mounted != NULL || dev->dev == root->dev || dev->ino != 1)
        panic("VFS self-test: /dev is not devfs's root\n");
    if (vfs_expect_stat("/dev", MIHOS_S_IFDIR | 0755) != 0)
        panic("VFS self-test: devfs's root has an mtime\n");

    // Down through the mount and back up out of it
    vfs_expect_same("/dev", "/dev/.", VFS_DIRECTORY);
    vfs_expect_same("/dev", "//dev//", VFS_DIRECTORY);
    vfs_expect_same("/dev", "/usr/../dev", VFS_DIRECTORY);
    vfs_expect_same("/", "/dev/..", VFS_DIRECTORY);
    vfs_expect_same("/", "/dev/../dev/..", VFS_DIRECTORY);
    vfs_expect_same(HELLO, "/dev/../usr/tests/hello", VFS_FILE);
    vfs_expect_error("/dev/nope", -ENOENT);
    vfs_expect_error("/dev/../nope", -ENOENT);

    // devfs lists its dots ("..": its own root; lookup is what steps out), then the
    // devices in registration order
    vfs_dirent_t entry;
    vfs_node_t *null = vfs_expect("/dev/null", VFS_CHARDEV);
    vfs_node_t *zero = vfs_expect("/dev/zero", VFS_CHARDEV);
    vfs_expect_entry(dev, 0, ".", VFS_DIRECTORY, dev->ino);
    vfs_expect_entry(dev, 1, "..", VFS_DIRECTORY, dev->ino);
    vfs_expect_entry(dev, 2, "null", VFS_CHARDEV, null->ino);
    vfs_expect_entry(dev, 3, "zero", VFS_CHARDEV, zero->ino);
    vfs_expect_entry(dev, 4, "console", VFS_CHARDEV, 0); // registered by console_init, after mem
    if (vfs_readdir(dev, 5, &entry) != 0 || vfs_readdir(dev, 100, &entry) != 0)
        panic("VFS self-test: /dev lists more than its dots, null, zero and console\n");
    vfs_node_put(zero);
    vfs_node_put(null);

    // Refused mounts change nothing
    if (vfs_mount("/sbin/init", dev) != -ENOTDIR)
        panic("VFS self-test: mounting on a file did not return -ENOTDIR\n");
    if (vfs_mount("/", dev) != -EBUSY)
        panic("VFS self-test: mounting on / did not return -EBUSY\n");
    if (vfs_mount("/dev", dev) != -EBUSY)
        panic("VFS self-test: mounting on /dev twice did not return -EBUSY\n");
    if (vfs_mount("/nope", dev) != -ENOENT)
        panic("VFS self-test: mounting on a missing path did not return -ENOENT\n");
    if (root->mounted != NULL || dev->mounted != NULL)
        panic("VFS self-test: a refused mount was recorded\n");

    if (dev->refs != dev_refs)
        panic("VFS self-test: devfs's root references leaked (%u → %u)\n", dev_refs, dev->refs);

    vfs_node_put(dev);
    vfs_node_put(root);
}

/**
 * Opens a device with an access mode; the file must get exactly `access`.
 */
static struct file *vfs_expect_device_open(const char *path, int flags, unsigned int access)
{
    struct file *file;
    int ret = vfs_open(path, flags, &file);
    if (ret != 0)
        panic("VFS self-test: opening %s (flags %d) failed: %d\n", path, flags, ret);
    if (file->flags != access || file->node == NULL || file->node->type != VFS_CHARDEV ||
        file->ops != file->node->device_ops)
        panic("VFS self-test: %s (flags %d) opened with the wrong flags or ops\n", path, flags);
    return file;
}

static void vfs_test_devices(void)
{
    vfs_node_t *null = vfs_expect("/dev/null", VFS_CHARDEV);
    vfs_node_t *zero = vfs_expect("/dev/zero", VFS_CHARDEV);
    unsigned int null_refs = null->refs;
    unsigned int zero_refs = zero->refs;

    // stat: a character device on devfs, with its device number and no size
    struct mihos_stat st;
    vfs_stat(null, &st);
    if (st.st_mode != (MIHOS_S_IFCHR | 0666) || st.st_rdev != MIHOS_MKDEV(1, 3) || st.st_size != 0 ||
        st.st_dev != null->dev || st.st_dev == 0)
        panic("VFS self-test: stat of /dev/null is wrong (mode 0%o)\n", st.st_mode);
    vfs_stat(zero, &st);
    if (st.st_mode != (MIHOS_S_IFCHR | 0666) || MIHOS_MAJOR(st.st_rdev) != 1 || MIHOS_MINOR(st.st_rdev) != 5)
        panic("VFS self-test: stat of /dev/zero is wrong (mode 0%o)\n", st.st_mode);

    // Each access mode becomes the file's flags; the ops are the driver's
    char buffer[32];
    struct file *file = vfs_expect_device_open("/dev/null", O_RDONLY, FILE_READ);
    if (file->ops->read(file, buffer, sizeof(buffer)) != 0 || file->ops->read(file, buffer, sizeof(buffer)) != 0)
        panic("VFS self-test: reading /dev/null did not give 0\n");
    file_put(file);

    file = vfs_expect_device_open("/dev/null", O_WRONLY, FILE_WRITE);
    if (file->ops->write(file, "discarded", 9) != 9)
        panic("VFS self-test: writing /dev/null did not take everything\n");
    file_put(file);

    file = vfs_expect_device_open("/dev/zero", O_RDWR, FILE_READ | FILE_WRITE);
    memset(buffer, 0xA5, sizeof(buffer));
    if (file->ops->read(file, buffer, sizeof(buffer)) != (long)sizeof(buffer))
        panic("VFS self-test: reading /dev/zero gave a short count\n");
    for (size_t i = 0; i < sizeof(buffer); i++)
        if (buffer[i] != 0)
            panic("VFS self-test: /dev/zero gave a non-zero byte at %zu\n", i);
    if (file->ops->write(file, buffer, 5) != 5)
        panic("VFS self-test: writing /dev/zero did not take everything\n");
    file_put(file);

    // Not terminals: no ioctl at all, so sys_ioctl answers ENOTTY
    if (null->device_ops->ioctl != NULL || zero->device_ops->ioctl != NULL)
        panic("VFS self-test: /dev/null or /dev/zero has an ioctl\n");

    // A device is no directory; the access mode 3 means nothing
    if (vfs_open("/dev/null", O_RDONLY | O_DIRECTORY, &file) != -ENOTDIR)
        panic("VFS self-test: O_DIRECTORY on /dev/null did not return -ENOTDIR\n");
    if (vfs_open("/dev/null", O_ACCMODE, &file) != -EINVAL)
        panic("VFS self-test: access mode 3 on /dev/null did not return -EINVAL\n");
    vfs_dirent_t entry;
    if (vfs_readdir(null, 0, &entry) != -ENOTDIR)
        panic("VFS self-test: readdir of /dev/null did not return -ENOTDIR\n");
    vfs_expect_error("/dev/null/x", -ENOTDIR);
    vfs_expect_error("/dev/null/", -ENOTDIR);

    // Initrd files stay read-only; a missing path is ENOENT before any access check
    if (vfs_open(HELLO, O_WRONLY, &file) != -EROFS || vfs_open("/nope", O_WRONLY, &file) != -ENOENT)
        panic("VFS self-test: write access outside /dev gave the wrong error\n");

    // Registration refuses duplicates and bad names, and changes nothing then
    if (devfs_register("null", 0666, MIHOS_MKDEV(1, 3), null->device_ops) != -EEXIST)
        panic("VFS self-test: registering null twice did not return -EEXIST\n");
    if (devfs_register("", 0666, 0, null->device_ops) != -EINVAL ||
        devfs_register("a/b", 0666, 0, null->device_ops) != -EINVAL ||
        devfs_register(NULL, 0666, 0, null->device_ops) != -EINVAL)
        panic("VFS self-test: a bad device name was not refused\n");
    vfs_expect_error("/dev/a", -ENOENT);

    // Every open and close above took and dropped exactly one reference
    if (null->refs != null_refs || zero->refs != zero_refs)
        panic("VFS self-test: device references leaked (null %u → %u, zero %u → %u)\n", null_refs, null->refs,
              zero_refs, zero->refs);

    vfs_node_put(zero);
    vfs_node_put(null);
}

void vfs_selftest(void)
{
    // Every lookup returns a reference and every step drops one: counts must not move
    vfs_node_t *root = vfs_expect("/", VFS_DIRECTORY);
    vfs_node_t *hello = vfs_expect(HELLO, VFS_FILE);
    unsigned int root_refs = root->refs;
    unsigned int hello_refs = hello->refs;

    vfs_test_lookup();
    vfs_test_read();
    vfs_test_readdir();
    vfs_test_getdents();
    vfs_test_stat();
    vfs_test_mount();
    vfs_test_devices();

    if (root->refs != root_refs || hello->refs != hello_refs)
        panic("VFS self-test: references leaked (root %u → %u, hello %u → %u)\n", root_refs, root->refs,
              hello_refs, hello->refs);

    vfs_node_put(hello);
    vfs_node_put(root);
}
