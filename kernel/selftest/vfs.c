#include <selftest/vfs.h>
#include <fs/vfs.h>
#include <mm/kmalloc.h>
#include <mihos/errno.h>
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

void vfs_selftest(void)
{
    // Every lookup returns a reference and every step drops one: counts must not move
    vfs_node_t *root = vfs_expect("/", VFS_DIRECTORY);
    vfs_node_t *hello = vfs_expect(HELLO, VFS_FILE);
    unsigned int root_refs = root->refs;
    unsigned int hello_refs = hello->refs;

    vfs_test_lookup();
    vfs_test_read();

    if (root->refs != root_refs || hello->refs != hello_refs)
        panic("VFS self-test: references leaked (root %u → %u, hello %u → %u)\n", root_refs, root->refs,
              hello_refs, hello->refs);

    vfs_node_put(hello);
    vfs_node_put(root);
}
