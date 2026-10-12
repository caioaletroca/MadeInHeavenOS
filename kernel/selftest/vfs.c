#include <selftest/vfs.h>
#include <fs/vfs.h>
#include <fs/file.h>
#include <mm/kmalloc.h>
#include <mihos/errno.h>
#include <mihos/fcntl.h>
#include <mihos/dirent.h>
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

    // The root: "." and ".." are both itself, then the archive's directories in order
    vfs_expect_entry(root, 0, ".", VFS_DIRECTORY, root->ino);
    vfs_expect_entry(root, 1, "..", VFS_DIRECTORY, root->ino);
    vfs_expect_entry(root, 2, "bin", VFS_DIRECTORY, 0);
    vfs_expect_entry(root, 3, "sbin", VFS_DIRECTORY, 0);
    vfs_expect_entry(root, 4, "usr", VFS_DIRECTORY, usr->ino);
    if (vfs_readdir(root, 5, &entry) != 0 || vfs_readdir(root, 100, &entry) != 0)
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
static const char *const root_names[] = {".", "..", "bin", "sbin", "usr"};
#define ROOT_RECORDS 80 // 5 records of 16 bytes: 11-byte header + name + '\0', rounded to 8

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
        if (n != ROOT_RECORDS || next != 5)
            panic("VFS self-test: getdents of / returned %ld, next %u\n", n, (unsigned int)next);

        size_t used = 0;
        for (size_t i = 0; i < 5; i++)
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
        if (vfs_file_getdents(file, buffer, sizeof(buffer), &next) != 0 || next != 5)
            panic("VFS self-test: getdents at the end did not return 0\n");

        // "sbin" fills its 16 bytes exactly: nothing may be written after them
        file->offset = 3;
        memset(buffer, 0, sizeof(buffer));
        buffer[16] = 0xA5;
        n = vfs_file_getdents(file, buffer, 16, &next);
        if (n != 16 || next != 4 || buffer[16] != 0xA5)
            panic("VFS self-test: getdents into an exact fit returned %ld, next %u, canary 0x%x\n", n,
                  (unsigned int)next, buffer[16]);

        // Not even one record fits
        file->offset = 0;
        if (vfs_file_getdents(file, buffer, 15, &next) != -EINVAL)
            panic("VFS self-test: getdents into 15 bytes did not return -EINVAL\n");
    }

    file_put(file);
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

    if (root->refs != root_refs || hello->refs != hello_refs)
        panic("VFS self-test: references leaked (root %u → %u, hello %u → %u)\n", root_refs, root->refs,
              hello_refs, hello->refs);

    vfs_node_put(hello);
    vfs_node_put(root);
}
