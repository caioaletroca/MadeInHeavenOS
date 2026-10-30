#include <selftest/ext2.h>
#include <fs/ext2.h>
#include <fs/vfs.h>
#include <fs/file.h>
#include <driver/block.h>
#include <driver/block_cache.h>
#include <mm/kmalloc.h>
#include <mihos/errno.h>
#include <mihos/fcntl.h>
#include <mihos/stat.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <panic.h>
#include <util.h>

/*
 * Against disk.img (Makefile): 1 KiB blocks, mounted on /mnt by ext2_init.
 * The layout comes from what mke2fs writes; offsets of on-disk fields are the
 * ext2 spec's, written out here on purpose (not taken from fs/ext2/disk_format.h).
 */
#define MNT "/mnt"
#define BIG "/mnt/big.txt"
#define BIG_LINES 38400
#define BIG_SIZE (BIG_LINES * 8) // lines "%07d\n": byte offset = 8 * line
#define SPARSE_SIZE 16387        // "start", a hole up to 16384, "end"
#define HELLO_TEXT "Hello from the disk!\n"
#define NESTED_TEXT "Three directories down.\n"
#define LONG_TEXT "The longest name.\n"
#define LONG_NAME_LENGTH 255
#define BLOCK 1024
#define MTIME_MIN 1700000000 // a plausible build time: after 2023

// On-disk offsets used to build broken copies
#define SB 1024                 // the superblock's byte on the disk
#define SB_INODES_COUNT 0
#define SB_BLOCKS_COUNT 4
#define SB_LOG_BLOCK_SIZE 24
#define SB_INODES_PER_GROUP 40
#define SB_MAGIC 56
#define SB_REV_LEVEL 76
#define SB_INODE_SIZE 88
#define SB_FEATURE_INCOMPAT 96
#define SB_FEATURE_RO_COMPAT 100
#define SB_FREE_BLOCKS_COUNT 12
#define SB_FREE_INODES_COUNT 16
#define SB_STATE 58
#define SB_WTIME 48
#define EXT2_TEST_VALID_FS 0x0001 // s_state: cleanly unmounted
#define BG_INODE_BITMAP 4
#define BG_FREE_BLOCKS_COUNT 12
#define BG_FREE_INODES_COUNT 14
#define BG_USED_DIRS_COUNT 16
#define EXT2_TEST_DIRECT 12 // i_block's direct pointers
#define GROUP_TABLE (2 * BLOCK) // block 2 with 1 KiB blocks
#define BG_INODE_TABLE 8
#define I_MODE 0
#define I_SIZE 4
#define I_MTIME 16
#define I_DTIME 20
#define I_LINKS_COUNT 26
#define I_BLOCKS 28
#define I_BLOCK 40
#define I_EXTRA_ISIZE 128 // large inodes
#define DIRENT_REC_LEN 4
#define DIRENT_NAME_LEN 6

static char long_path[sizeof(MNT "/") + LONG_NAME_LENGTH + 1];

// "/" is an ext2 on a disk (root=), writable, not the read-only initrd: the
// refusals that name the initrd's files are checked only in the live boot
static bool root_on_disk;

static vfs_node_t *ext2_expect(const char *path, vfs_node_type_t type)
{
    vfs_node_t *node = NULL;
    int ret = vfs_lookup(path, &node);
    if (ret != 0)
        panic("ext2 self-test: lookup of '%s' failed: %d\n", path, ret);
    if (node->type != type)
        panic("ext2 self-test: '%s' has the wrong type\n", path);
    return node;
}

static void ext2_expect_error(const char *path, int error)
{
    vfs_node_t *node = NULL;
    int ret = vfs_lookup(path, &node);
    if (ret != error || node != NULL)
        panic("ext2 self-test: '%s' returned %d instead of %d\n", path, ret, error);
}

// A small file read whole equals `text`, and reading at its end gives 0
static void ext2_expect_text(const char *path, const char *text)
{
    vfs_node_t *node = ext2_expect(path, VFS_FILE);
    size_t length = strlen(text);
    char buffer[64];
    memset(buffer, 0xA5, sizeof(buffer));

    if (node->size != length || vfs_read(node, buffer, sizeof(buffer), 0) != (long)length ||
        memcmp(buffer, text, length) != 0 || (unsigned char)buffer[length] != 0xA5)
        panic("ext2 self-test: '%s' does not read back its text\n", path);
    if (vfs_read(node, buffer, sizeof(buffer), length) != 0)
        panic("ext2 self-test: reading '%s' at its end did not give 0\n", path);

    vfs_node_put(node);
}

// Line `line` of big.txt: "%07d\n" holding its own number
static bool big_line_ok(const char *p, uint32_t line)
{
    uint32_t value = 0;
    for (int i = 0; i < 7; i++)
    {
        if (p[i] < '0' || p[i] > '9')
            return false;
        value = value * 10 + (uint32_t)(p[i] - '0');
    }
    return p[7] == '\n' && value == line;
}

/**
 * The mounted root: a directory on its own file system, ".." leading out of
 * the mount, and the listing in disk order (the symlink skipped).
 */
static void ext2_test_root(void)
{
    vfs_node_t *root = ext2_expect("/", VFS_DIRECTORY);
    vfs_node_t *mnt = ext2_expect(MNT, VFS_DIRECTORY);

    if (mnt->ino != 2 || mnt->dev == 0 || mnt->dev == root->dev || mnt->mount_point == NULL)
        panic("ext2 self-test: /mnt is not inode 2 of a mounted file system\n");

    struct mihos_stat st;
    vfs_stat(mnt, &st);
    if (st.st_mode != (MIHOS_S_IFDIR | 0755) || st.st_ino != 2 || st.st_dev != mnt->dev || st.st_mtim.tv_sec < MTIME_MIN)
        panic("ext2 self-test: stat of /mnt is wrong (mode 0%o)\n", st.st_mode);

    // ".." of the mounted root is the directory above the mount point
    vfs_node_t *up = ext2_expect(MNT "/..", VFS_DIRECTORY);
    if (up != root)
        panic("ext2 self-test: /mnt/.. is not /\n");
    vfs_node_put(up);

    static const struct
    {
        const char *name;
        vfs_node_type_t type;
    } listing[] = {
        {".", VFS_DIRECTORY}, {"..", VFS_DIRECTORY}, {"lost+found", VFS_DIRECTORY}, {NULL, VFS_FILE}, // the long name
        {"big.txt", VFS_FILE}, {"docs", VFS_DIRECTORY}, {"empty", VFS_FILE},  {"grow", VFS_DIRECTORY},
        {"hello.txt", VFS_FILE}, {"sparse", VFS_FILE},
    };
    size_t count = sizeof(listing) / sizeof(listing[0]);
    uint32_t subdirectories = 0;

    for (size_t i = 0; i < count; i++)
    {
        vfs_dirent_t entry;
        entry.length = (size_t)-1;
        if (vfs_readdir(mnt, i, &entry) != 1)
            panic("ext2 self-test: /mnt has no entry %zu\n", i);

        const char *name = listing[i].name != NULL ? listing[i].name : long_path + sizeof(MNT);
        if (entry.length != strlen(name) || strcmp(entry.name, name) != 0 || entry.type != listing[i].type)
            panic("ext2 self-test: entry %zu of /mnt is '%s', not '%s'\n", i, entry.name, name);
        if (i < 2 && entry.ino != 2)
            panic("ext2 self-test: '%s' of /mnt is not inode 2\n", name);

        subdirectories += i >= 2 && listing[i].type == VFS_DIRECTORY;
    }

    vfs_dirent_t entry;
    if (vfs_readdir(mnt, count, &entry) != 0 || vfs_readdir(mnt, 100, &entry) != 0)
        panic("ext2 self-test: /mnt lists more than expected (is 'link' shown?)\n");

    // Every subdirectory's ".." links to the root: 2 + one per subdirectory
    if (mnt->nlink != 2 + subdirectories)
        panic("ext2 self-test: /mnt has %u links, expected %u\n", mnt->nlink, 2 + subdirectories);

    // readdir and lookup agree on inode numbers
    vfs_node_t *hello = ext2_expect(MNT "/hello.txt", VFS_FILE);
    bool found = false;
    for (uint64_t i = 0; vfs_readdir(mnt, i, &entry) == 1; i++)
        found = found || (strcmp(entry.name, "hello.txt") == 0 && entry.ino == hello->ino);
    if (!found)
        panic("ext2 self-test: readdir and lookup disagree on hello.txt's inode\n");
    vfs_node_put(hello);

    vfs_node_put(mnt);
    vfs_node_put(root);
}

/**
 * Lookups: names compared exactly, what the VFS cannot show is not found,
 * paths through ".." inside the file system reach the same node.
 */
static void ext2_test_lookup(void)
{
    ext2_expect_error(MNT "/link", -ENOENT); // a symlink: no VFS type for it yet
    ext2_expect_error(MNT "/nope", -ENOENT);
    ext2_expect_error(MNT "/hello", -ENOENT);     // a prefix
    ext2_expect_error(MNT "/hello.txtx", -ENOENT); // longer
    ext2_expect_error(MNT "/HELLO.TXT", -ENOENT);  // case matters
    ext2_expect_error(MNT "/hello.txt/x", -ENOTDIR);
    ext2_expect_error(MNT "/hello.txt/", -ENOTDIR);

    // The longest name works; one more character is the VFS's ENAMETOOLONG
    vfs_node_put(ext2_expect(long_path, VFS_FILE));
    char too_long[sizeof(long_path) + 1];
    size_t length = strlen(long_path);
    memcpy(too_long, long_path, length);
    memcpy(too_long + length, "a", 2); // one more 'a', then '\0'
    ext2_expect_error(too_long, -ENAMETOOLONG);

    vfs_node_t *hello = ext2_expect(MNT "/hello.txt", VFS_FILE);
    vfs_node_t *again = ext2_expect(MNT "/docs/deep/../../hello.txt", VFS_FILE);
    vfs_node_t *docs = ext2_expect(MNT "/docs", VFS_DIRECTORY);
    vfs_node_t *docs_up = ext2_expect(MNT "/docs/deep/nested/../..", VFS_DIRECTORY);
    if (again != hello || docs_up != docs)
        panic("ext2 self-test: paths through '..' reached other nodes\n");
    vfs_node_put(docs_up);
    vfs_node_put(docs);
    vfs_node_put(again);
    vfs_node_put(hello);
}

/**
 * The node cache: one node per inode while it is held, children holding their
 * directory, and everything let go again when the last reference goes.
 */
static void ext2_test_nodes(void)
{
    vfs_node_t *mnt = ext2_expect(MNT, VFS_DIRECTORY);
    unsigned int mnt_refs = mnt->refs;

    vfs_node_t *a = ext2_expect(MNT "/hello.txt", VFS_FILE);
    vfs_node_t *b = ext2_expect(MNT "/hello.txt", VFS_FILE);
    if (a != b || a->refs != 2 || a->parent != mnt || mnt->refs != mnt_refs + 1)
        panic("ext2 self-test: two lookups of hello.txt are not one node held twice\n");
    vfs_node_put(b);
    uint64_t ino = a->ino;
    vfs_node_put(a); // the last reference: released and freed

    if (mnt->refs != mnt_refs)
        panic("ext2 self-test: releasing hello.txt did not let /mnt go (%u, was %u)\n", mnt->refs, mnt_refs);

    // Looked up again after its release: a fresh node for the same inode
    a = ext2_expect(MNT "/hello.txt", VFS_FILE);
    if (a->ino != ino || a->refs != 1)
        panic("ext2 self-test: hello.txt after its release is not inode %u with one reference\n", (unsigned int)ino);
    vfs_node_put(a);

    // A deep file holds its whole chain of directories; letting it go releases them all
    vfs_node_t *docs = ext2_expect(MNT "/docs", VFS_DIRECTORY);
    unsigned int docs_refs = docs->refs;
    if (mnt->refs != mnt_refs + 1)
        panic("ext2 self-test: a held /mnt/docs does not hold /mnt\n");
    vfs_node_t *file = ext2_expect(MNT "/docs/deep/nested/file.txt", VFS_FILE);
    vfs_node_t *nested = file->parent;
    if (nested->parent == NULL || nested->parent->parent != docs || docs->refs != docs_refs + 1)
        panic("ext2 self-test: file.txt does not hold its directories up to /mnt/docs\n");
    vfs_node_put(file);
    if (docs->refs != docs_refs)
        panic("ext2 self-test: releasing file.txt left its directories held\n");
    vfs_node_put(docs);
    if (mnt->refs != mnt_refs)
        panic("ext2 self-test: releasing /mnt/docs did not let /mnt go\n");

    vfs_node_put(mnt);
}

/**
 * Reads: small files, the empty file, big.txt through direct, single and
 * double indirect blocks, and the sparse file's hole reading as zeros.
 */
static void ext2_test_read(void)
{
    ext2_expect_text(MNT "/hello.txt", HELLO_TEXT);
    ext2_expect_text(MNT "/docs/deep/nested/file.txt", NESTED_TEXT);
    ext2_expect_text(long_path, LONG_TEXT);

    vfs_node_t *empty = ext2_expect(MNT "/empty", VFS_FILE);
    char byte = 'x';
    if (empty->size != 0 || vfs_read(empty, &byte, 1, 0) != 0 || byte != 'x')
        panic("ext2 self-test: the empty file is not empty\n");
    vfs_node_put(empty);

    vfs_node_t *big = ext2_expect(BIG, VFS_FILE);
    if (big->size != BIG_SIZE || big->mode != 0644 || big->nlink != 1 || big->mtime < MTIME_MIN)
        panic("ext2 self-test: big.txt's size, mode, links or time are wrong\n");

    char *data = kmalloc(BIG_SIZE + 1);
    char *part = kmalloc(2 * BLOCK + 16);
    if (data == NULL || part == NULL)
        panic("ext2 self-test: out of memory\n");

    // Whole, in one read: every line in its place
    data[BIG_SIZE] = 0x5A;
    if (vfs_read(big, data, BIG_SIZE + 100, 0) != BIG_SIZE || data[BIG_SIZE] != 0x5A)
        panic("ext2 self-test: reading big.txt whole did not give exactly its size\n");
    for (uint32_t line = 0; line < BIG_LINES; line++)
        if (!big_line_ok(data + 8 * line, line))
            panic("ext2 self-test: line %u of big.txt is wrong (block %u)\n", line, line * 8 / BLOCK);

    // Pieces across every kind of boundary equal the whole read
    static const struct
    {
        uint32_t offset;
        uint32_t count;
    } pieces[] = {
        {0, 1},                         // the first byte
        {BLOCK - 3, 10},                // across blocks 0 and 1
        {12 * BLOCK - 5, 10},           // the last direct block into the first single-indirect one
        {12 * BLOCK, BLOCK},            // exactly the first single-indirect block
        {268 * BLOCK - 7, 2 * BLOCK},   // the last single-indirect block into the double-indirect ones
        {280 * BLOCK + 100, BLOCK + 7}, // inside the double-indirect range
        {BIG_SIZE - 5, 100},            // across the end: only 5 bytes
    };
    for (size_t i = 0; i < sizeof(pieces) / sizeof(pieces[0]); i++)
    {
        uint32_t expected = pieces[i].count;
        if (pieces[i].offset + expected > BIG_SIZE)
            expected = BIG_SIZE - pieces[i].offset;

        memset(part, 0xA5, 2 * BLOCK + 16);
        long n = vfs_read(big, part, pieces[i].count, pieces[i].offset);
        if (n != (long)expected || memcmp(part, data + pieces[i].offset, expected) != 0 ||
            (unsigned char)part[expected] != 0xA5)
            panic("ext2 self-test: %u bytes of big.txt at %u read %ld or different bytes\n", pieces[i].count,
                  pieces[i].offset, n);
    }
    if (vfs_read(big, part, 10, BIG_SIZE) != 0 || vfs_read(big, part, 10, UINT64_MAX) != 0)
        panic("ext2 self-test: reading at or past the end of big.txt did not give 0\n");
    vfs_node_put(big);

    // The hole: never written, reads as zeros (the buffer is filled with 0xA5 first)
    vfs_node_t *sparse = ext2_expect(MNT "/sparse", VFS_FILE);
    memset(data, 0xA5, SPARSE_SIZE + 1);
    if (sparse->size != SPARSE_SIZE || vfs_read(sparse, data, SPARSE_SIZE + 1, 0) != SPARSE_SIZE)
        panic("ext2 self-test: the sparse file does not read back its size\n");
    if (memcmp(data, "start", 5) != 0 || memcmp(data + 16384, "end", 3) != 0 || (unsigned char)data[SPARSE_SIZE] != 0xA5)
        panic("ext2 self-test: the sparse file's ends are wrong\n");
    for (uint32_t i = 5; i < 16384; i++)
        if (data[i] != 0)
            panic("ext2 self-test: byte %u of the sparse file's hole is not zero\n", i);
    vfs_node_put(sparse);

    kfree(part);
    kfree(data);

    // Files open for writing (the disk's ro_compat features are all known), directories
    // open, reading one is EISDIR
    struct file *file;
    if (vfs_open(MNT "/hello.txt", O_RDWR, 0, &file) != 0 || file->flags != (FILE_READ | FILE_WRITE))
        panic("ext2 self-test: opening hello.txt for reading and writing failed\n");
    file_put(file);
    if (vfs_open(MNT "/docs", O_RDONLY | O_DIRECTORY, 0, &file) != 0)
        panic("ext2 self-test: opening /mnt/docs as a directory failed\n");
    if (vfs_read(file->node, &byte, 1, 0) != -EISDIR)
        panic("ext2 self-test: reading a directory did not return -EISDIR\n");
    file_put(file);
}

static void ext2_truncate_or_panic(vfs_node_t *node, uint64_t size)
{
    int ret = vfs_truncate(node, size);
    if (ret != 0)
        panic("ext2 self-test: truncating to %u failed: %d\n", (unsigned int)size, ret);
}

static void ext2_write_or_panic(vfs_node_t *node, const void *buffer, size_t count, uint64_t offset)
{
    long n = vfs_write(node, buffer, count, offset);
    if (n != (long)count)
        panic("ext2 self-test: writing %zu bytes at %u wrote %ld\n", count, (unsigned int)offset, n);
}

/**
 * Overwriting file data in place: big.txt across every kind of block
 * boundary, read back through the node with its neighbours unchanged, and
 * sparse's two blocks. Everything is written back as it was and no size
 * changes: the ext2 test reads these files after this (growing is
 * ext2_test_grow's, on the grow/ files).
 */
static void ext2_test_write(void)
{
    vfs_node_t *big = ext2_expect(BIG, VFS_FILE);
    char *data = kmalloc(BIG_SIZE);
    char *pattern = kmalloc(2 * BLOCK + 16);
    char *back = kmalloc(2 * BLOCK + 16);
    if (data == NULL || pattern == NULL || back == NULL)
        panic("ext2 self-test: out of memory\n");
    if (vfs_read(big, data, BIG_SIZE, 0) != BIG_SIZE)
        panic("ext2 self-test: reading big.txt failed\n");

    static const struct
    {
        uint32_t offset; // never 0 and never at the end: the bytes around it are checked
        uint32_t count;
    } pieces[] = {
        {100, 1},                       // one byte
        {BLOCK - 3, 10},                // across blocks 0 and 1
        {12 * BLOCK - 5, 10},           // the last direct block into the first single-indirect one
        {12 * BLOCK, BLOCK},            // exactly one single-indirect block
        {268 * BLOCK - 7, 2 * BLOCK},   // the last single-indirect block into the double-indirect ones
        {280 * BLOCK + 100, BLOCK + 7}, // inside the double-indirect range
    };
    for (size_t i = 0; i < sizeof(pieces) / sizeof(pieces[0]); i++)
    {
        uint32_t offset = pieces[i].offset, count = pieces[i].count;
        for (uint32_t b = 0; b < count; b++)
            pattern[b] = (char)('A' + (i * 7 + b) % 26);

        ext2_write_or_panic(big, pattern, count, offset);
        if (vfs_read(big, back, count + 2, offset - 1) != (long)count + 2 || back[0] != data[offset - 1] ||
            memcmp(back + 1, pattern, count) != 0 || back[count + 1] != data[offset + count])
            panic("ext2 self-test: %u bytes written to big.txt at %u read back wrong\n", count, offset);

        ext2_write_or_panic(big, data + offset, count, offset); // put the lines back
    }

    // The last 5 bytes, exactly up to the end (no growing); 0 bytes is 0 anywhere, even at the
    // end; past the largest file the file system holds: -EFBIG, nothing written
    for (uint32_t b = 0; b < 5; b++)
        pattern[b] = (char)('a' + b);
    ext2_write_or_panic(big, pattern, 5, BIG_SIZE - 5);
    if (vfs_read(big, back, 10, BIG_SIZE - 5) != 5 || memcmp(back, pattern, 5) != 0)
        panic("ext2 self-test: the last 5 bytes of big.txt read back wrong\n");
    ext2_write_or_panic(big, data + BIG_SIZE - 5, 5, BIG_SIZE - 5);
    if (vfs_write(big, pattern, 0, 1000) != 0 || vfs_write(big, pattern, 0, BIG_SIZE) != 0)
        panic("ext2 self-test: a 0-byte write to big.txt did not return 0\n");
    if (vfs_write(big, pattern, 1, VFS_APPEND - 1) != -EFBIG) // VFS_APPEND itself (UINT64_MAX) appends
        panic("ext2 self-test: a write at UINT64_MAX - 1 did not return -EFBIG\n");

    // Every line as it was, the size unchanged
    if (big->size != BIG_SIZE || vfs_read(big, data, BIG_SIZE, 0) != BIG_SIZE)
        panic("ext2 self-test: big.txt changed size\n");
    for (uint32_t line = 0; line < BIG_LINES; line++)
        if (!big_line_ok(data + 8 * line, line))
            panic("ext2 self-test: line %u of big.txt was not restored\n", line);
    vfs_node_put(big);

    // The sparse file: block 0 ("start") and block 16 ("end") exist, 1..15 are a hole
    vfs_node_t *sparse = ext2_expect(MNT "/sparse", VFS_FILE);
    if (vfs_read(sparse, data, BLOCK, 0) != BLOCK)
        panic("ext2 self-test: reading the sparse file failed\n");
    ext2_write_or_panic(sparse, "START", 5, 0);
    ext2_write_or_panic(sparse, "1234", 4, BLOCK - 4); // the end of block 0, up to the hole
    ext2_write_or_panic(sparse, "END", 3, 16384);
    if (vfs_read(sparse, back, 5, 0) != 5 || memcmp(back, "START", 5) != 0 ||
        vfs_read(sparse, back, 4, BLOCK - 4) != 4 || memcmp(back, "1234", 4) != 0 ||
        vfs_read(sparse, back, 3, 16384) != 3 || memcmp(back, "END", 3) != 0)
        panic("ext2 self-test: the sparse file's written blocks read back wrong\n");
    if (vfs_read(sparse, back, 10, 2 * BLOCK) != 10 || memcmp(back, "\0\0\0\0\0\0\0\0\0\0", 10) != 0)
        panic("ext2 self-test: the sparse file's hole is no longer zeros\n");
    ext2_write_or_panic(sparse, data, BLOCK, 0);
    ext2_write_or_panic(sparse, "end", 3, 16384);
    if (sparse->size != SPARSE_SIZE)
        panic("ext2 self-test: the sparse file changed size\n");
    vfs_node_put(sparse);

    // 0 bytes to an empty file is 0 and leaves it empty; a directory is never written
    vfs_node_t *empty = ext2_expect(MNT "/empty", VFS_FILE);
    if (vfs_write(empty, "x", 0, 0) != 0 || empty->size != 0)
        panic("ext2 self-test: a 0-byte write to the empty file did not return 0\n");
    vfs_node_put(empty);
    vfs_node_t *docs = ext2_expect(MNT "/docs", VFS_DIRECTORY);
    if (vfs_write(docs, "x", 1, 0) != -EISDIR)
        panic("ext2 self-test: writing a directory did not return -EISDIR\n");
    vfs_node_put(docs);

    // The other small files were not touched
    ext2_expect_text(MNT "/hello.txt", HELLO_TEXT);
    ext2_expect_text(MNT "/docs/deep/nested/file.txt", NESTED_TEXT);

    kfree(back);
    kfree(pattern);
    kfree(data);
}

/*
 * Broken disks: an "overlay" block device reads the test disk and replaces a
 * few bytes on the way. Each case has its own device, so the block cache
 * (keyed by device) never mixes their blocks. File systems that mount stay
 * allocated afterwards: there is no unmount yet.
 */
typedef struct overlay
{
    block_device_t *base;
    uint64_t offset; // byte on the disk
    uint32_t length; // 1, 2 or 4 bytes of `value` (little-endian, like ext2)
    uint32_t value;
} overlay_t;

static int overlay_read(block_device_t *dev, uint64_t sector, uint32_t count, void *buffer)
{
    const overlay_t *o = dev->private;
    int ret = block_read(o->base, sector, count, buffer);
    if (ret < 0)
        return ret;

    uint64_t start = sector * BLOCK_SECTOR_SIZE;
    uint64_t end = start + (uint64_t)count * BLOCK_SECTOR_SIZE;
    if (o->offset >= start && o->offset + o->length <= end)
        memcpy((uint8_t *)buffer + (o->offset - start), &o->value, o->length);
    return 0;
}

static const block_ops_t overlay_ops = {
    .read = overlay_read,
};

#define OVERLAYS 20
static overlay_t overlays[OVERLAYS];
static block_device_t overlay_devices[OVERLAYS];
static size_t overlays_used;

static block_device_t *overlay_create(block_device_t *base, uint64_t offset, uint32_t length, uint32_t value)
{
    if (overlays_used == OVERLAYS)
        panic("ext2 self-test: out of overlay devices\n");

    overlay_t *o = &overlays[overlays_used];
    block_device_t *dev = &overlay_devices[overlays_used++];
    *o = (overlay_t){.base = base, .offset = offset, .length = length, .value = value};
    memcpy(dev->name, "overlay", sizeof("overlay"));
    dev->sector_count = base->sector_count;
    dev->ops = &overlay_ops;
    dev->private = o;
    return dev;
}

static uint32_t disk_u32(block_device_t *disk, uint64_t offset)
{
    // Static, not on the stack: GCC inlines this into every caller, each copy with its own sector,
    // which took one test's frame to 15 KB of the 16 KB boot stack (the self-tests run on one thread)
    static uint8_t sector[BLOCK_SECTOR_SIZE];
    if (block_read(disk, offset / BLOCK_SECTOR_SIZE, 1, sector) != 0)
        panic("ext2 self-test: reading the test disk failed\n");

    uint32_t value;
    memcpy(&value, sector + offset % BLOCK_SECTOR_SIZE, sizeof(value));
    return value;
}

static void ext2_test_broken(void)
{
    block_device_t *disk = block_at(0);
    if (disk == NULL)
        panic("ext2 self-test: no disk\n");

    // Where the root inode and the root directory's first block are
    uint32_t inode_size = disk_u32(disk, SB + SB_INODE_SIZE) & 0xFFFF;
    uint32_t inode_table = disk_u32(disk, GROUP_TABLE + BG_INODE_TABLE);
    uint64_t root_inode = (uint64_t)inode_table * BLOCK + 1 * inode_size; // inode 2: slot 1
    uint64_t root_dir = (uint64_t)disk_u32(disk, root_inode + I_BLOCK) * BLOCK;

    // Unchanged: an overlay mounts and lists like the real thing, read-only (the device cannot write)
    vfs_node_t *root;
    if (ext2_mount(overlay_create(disk, 0, 0, 0), &root) != 0 || root->ino != 2)
        panic("ext2 self-test: an unchanged overlay does not mount\n");
    if (root->ops->write != NULL)
        panic("ext2 self-test: a disk that cannot write was mounted writable\n");
    vfs_node_put(root);

    // An unknown ro_compat feature: mounts and reads, but no node can be written
    if (ext2_mount(overlay_create(disk, SB + SB_FEATURE_RO_COMPAT, 4, 0x0003 | 0x0400), &root) != 0)
        panic("ext2 self-test: a disk with an unknown ro_compat feature did not mount\n");
    vfs_node_t *hello = NULL;
    char text[sizeof(HELLO_TEXT)];
    if (root->ops->lookup(root, "hello.txt", 9, &hello) != 0 || hello->ops->write != NULL ||
        vfs_write(hello, "x", 1, 0) != -EROFS)
        panic("ext2 self-test: a disk with an unknown ro_compat feature can be written\n");
    if (vfs_read(hello, text, sizeof(text), 0) != (long)strlen(HELLO_TEXT) || memcmp(text, HELLO_TEXT, strlen(HELLO_TEXT)) != 0)
        panic("ext2 self-test: a disk with an unknown ro_compat feature does not read\n");
    vfs_node_put(hello);
    vfs_node_put(root);

    static const struct
    {
        const char *what;
        uint64_t offset;
        uint32_t length;
        uint32_t value;
        int error;
    } refusals[] = {
        {"no magic", SB + SB_MAGIC, 2, 0x1234, -EINVAL},
        {"revision 0", SB + SB_REV_LEVEL, 4, 0, -EINVAL},
        {"an unknown incompat feature", SB + SB_FEATURE_INCOMPAT, 4, 0x0002 | 0x0040, -EINVAL},
        {"no FILETYPE", SB + SB_FEATURE_INCOMPAT, 4, 0, -EINVAL},
        {"8 KiB blocks", SB + SB_LOG_BLOCK_SIZE, 4, 3, -EINVAL},
        {"200-byte inodes", SB + SB_INODE_SIZE, 2, 200, -EINVAL},
        {"more blocks than the disk", SB + SB_BLOCKS_COUNT, 4, 100000, -EIO},
        {"no inodes per group", SB + SB_INODES_PER_GROUP, 4, 0, -EIO},
        {"more inodes than the groups hold", SB + SB_INODES_COUNT, 4, 100000, -EIO},
        {"an inode table at block 0", GROUP_TABLE + BG_INODE_TABLE, 4, 0, -EIO},
    };
    for (size_t i = 0; i < sizeof(refusals) / sizeof(refusals[0]); i++)
    {
        root = NULL;
        int ret = ext2_mount(overlay_create(disk, refusals[i].offset, refusals[i].length, refusals[i].value), &root);
        if (ret != refusals[i].error || root != NULL)
            panic("ext2 self-test: a disk with %s mounted or gave %d, not %d\n", refusals[i].what, ret,
                  refusals[i].error);
    }

    // A root inode that is a regular file: the mount reads it, then refuses
    root = NULL;
    if (ext2_mount(overlay_create(disk, root_inode + I_MODE, 2, 0x8000 | 0644), &root) != -EIO || root != NULL)
        panic("ext2 self-test: a root that is not a directory was mounted\n");

    // Corrupt directory records: the mount works (it does not read the root's
    // entries), listing and looking up give -EIO instead of looping or overrunning
    static const struct
    {
        const char *what;
        uint32_t offset; // in the root directory's first block
        uint32_t length;
        uint32_t value;
    } records[] = {
        {"a zero rec_len", DIRENT_REC_LEN, 2, 0},
        {"a rec_len not a multiple of 4", DIRENT_REC_LEN, 2, 13},
        {"a rec_len past the block", DIRENT_REC_LEN, 2, 2 * BLOCK},
        {"a name longer than its record", DIRENT_NAME_LEN, 1, 200},
    };
    for (size_t i = 0; i < sizeof(records) / sizeof(records[0]); i++)
    {
        block_device_t *dev = overlay_create(disk, root_dir + records[i].offset, records[i].length, records[i].value);
        if (ext2_mount(dev, &root) != 0)
            panic("ext2 self-test: a disk with %s in the root directory did not mount\n", records[i].what);

        vfs_dirent_t entry;
        vfs_node_t *child = NULL;
        if (vfs_readdir(root, 0, &entry) != -EIO || root->ops->lookup(root, "big.txt", 7, &child) != -EIO ||
            child != NULL)
            panic("ext2 self-test: %s in a directory did not give -EIO\n", records[i].what);
        vfs_node_put(root);
    }
}

// The free block counts on the disk itself (after a sync): the superblock's and group 0's
static uint32_t disk_free_blocks(block_device_t *disk, uint32_t *group0)
{
    if (block_cache_sync() != 0)
        panic("ext2 self-test: sync failed\n");
    *group0 = disk_u32(disk, GROUP_TABLE + BG_FREE_BLOCKS_COUNT) & 0xFFFF;
    return disk_u32(disk, SB + SB_FREE_BLOCKS_COUNT);
}

// A node's i_blocks and i_size as its inode on the disk holds them (after a sync)
static void disk_inode_sizes(block_device_t *disk, vfs_node_t *node, uint32_t *blocks, uint32_t *size)
{
    if (block_cache_sync() != 0)
        panic("ext2 self-test: sync failed\n");
    uint32_t inode_size = disk_u32(disk, SB + SB_INODE_SIZE) & 0xFFFF;
    uint64_t inode = (uint64_t)disk_u32(disk, GROUP_TABLE + BG_INODE_TABLE) * BLOCK + (node->ino - 1) * inode_size;
    *blocks = disk_u32(disk, inode + I_BLOCKS);
    *size = disk_u32(disk, inode + I_SIZE);
}

// After a write or a truncate: the inode on the disk holds the node's size and `blocks`, and
// exactly `used` blocks left the free counts (both of them; negative: came back) since *free /
// *group0; updates them
static void grow_expect(block_device_t *disk, vfs_node_t *node, uint32_t blocks, int32_t used, uint32_t *free,
                        uint32_t *group0, const char *what)
{
    uint32_t disk_blocks, disk_size, now_group0;
    disk_inode_sizes(disk, node, &disk_blocks, &disk_size);
    uint32_t now = disk_free_blocks(disk, &now_group0);
    if (disk_blocks != blocks || disk_size != (uint32_t)node->size)
        panic("ext2 self-test: %s: the inode holds i_blocks %u and size %u, expected %u and %u\n", what, disk_blocks,
              disk_size, blocks, (uint32_t)node->size);
    if ((int32_t)(*free - now) != used || (int32_t)(*group0 - now_group0) != used)
        panic("ext2 self-test: %s: %d blocks left the superblock's count and %d the group's, expected %d\n", what,
              (int32_t)(*free - now), (int32_t)(*group0 - now_group0), used);
    *free = now;
    *group0 = now_group0;
}

/**
 * Growing files (the grow/ files of the test disk, which no other test reads):
 * an append into a new direct block, holes filled directly and inside a
 * single-indirect table, an empty file growing past its direct blocks (a new
 * table), and an append far past the end needing a new second-level table.
 * Each write is read back; on the disk, after a sync, the inode holds the new
 * size and i_blocks (tables included, 2 per 1 KiB block) and exactly that many
 * blocks left both free counts.
 */
static uint32_t grow_free_before; // free blocks before ext2_test_grow, for ext2_test_truncate

static void ext2_test_grow(void)
{
    block_device_t *disk = block_at(0);
    char *pattern = kmalloc(13 * BLOCK);
    char *back = kmalloc(13 * BLOCK + 16);
    if (disk == NULL || pattern == NULL || back == NULL)
        panic("ext2 self-test: no disk, or out of memory\n");
    for (uint32_t b = 0; b < 13 * BLOCK; b++)
        pattern[b] = (char)('A' + b % 23);

    uint32_t group0;
    uint32_t free = disk_free_blocks(disk, &group0);
    grow_free_before = free;

    // small: 5 bytes in one block; 1100 more fill it and need one new block
    vfs_node_t *small = ext2_expect(MNT "/grow/small", VFS_FILE);
    ext2_write_or_panic(small, pattern, 1100, 5);
    if (small->size != 1105 || vfs_read(small, back, 2000, 0) != 1105 || memcmp(back, "small", 5) != 0 ||
        memcmp(back + 5, pattern, 1100) != 0)
        panic("ext2 self-test: grow/small does not read back its append\n");
    grow_expect(disk, small, 4, 1, &free, &group0, "an append into a new block");
    vfs_node_put(small);

    // holes: blocks 0 and 16 and the single-indirect table exist; logical 1 is a hole in the
    // inode's own pointers, logical 13 a hole inside the table
    vfs_node_t *holes = ext2_expect(MNT "/grow/holes", VFS_FILE);
    ext2_write_or_panic(holes, "X", 1, 2000);
    grow_expect(disk, holes, 8, 1, &free, &group0, "a hole in a direct pointer");
    ext2_write_or_panic(holes, "Y", 1, 13 * BLOCK + 10);
    grow_expect(disk, holes, 10, 1, &free, &group0, "a hole inside a single-indirect table");
    if (holes->size != SPARSE_SIZE || vfs_read(holes, back, 2 * BLOCK, BLOCK) != 2 * BLOCK)
        panic("ext2 self-test: grow/holes changed size or does not read\n");
    for (uint32_t b = 0; b < 2 * BLOCK; b++)
        if (back[b] != (b == 2000 - BLOCK ? 'X' : 0))
            panic("ext2 self-test: byte %u of grow/holes is wrong after filling a hole\n", BLOCK + b);
    if (vfs_read(holes, back, 12, 13 * BLOCK) != 12 || back[10] != 'Y' || back[9] != 0 || back[11] != 0)
        panic("ext2 self-test: the hole filled inside grow/holes's table reads back wrong\n");
    vfs_node_put(holes);

    // empty: 13 KiB at once, 12 direct blocks, then the single-indirect table and its first block
    vfs_node_t *empty = ext2_expect(MNT "/grow/empty", VFS_FILE);
    ext2_write_or_panic(empty, pattern, 13 * BLOCK, 0);
    if (empty->size != 13 * BLOCK || vfs_read(empty, back, 13 * BLOCK + 16, 0) != 13 * BLOCK ||
        memcmp(back, pattern, 13 * BLOCK) != 0)
        panic("ext2 self-test: grow/empty does not read back its 13 KiB\n");
    grow_expect(disk, empty, 28, 14, &free, &group0, "13 KiB into an empty file");

    // Past the largest file (16 GiB and a bit with 1 KiB blocks): -EFBIG, nothing allocated
    uint64_t largest = (uint64_t)(EXT2_TEST_DIRECT + 256 + 256 * 256 + 256 * 256 * 256) * BLOCK;
    if (vfs_write(empty, "x", 1, largest) != -EFBIG || empty->size != 13 * BLOCK)
        panic("ext2 self-test: a write past the largest file did not return -EFBIG\n");
    grow_expect(disk, empty, 28, 0, &free, &group0, "a write past the largest file");
    vfs_node_put(empty);

    // long: 300 blocks (direct, single, the first second-level table of the double); logical
    // 524 is the first block of the second second-level table: a new table and a data block,
    // the gap from the old end stays a hole
    vfs_node_t *grown = ext2_expect(MNT "/grow/long", VFS_FILE);
    ext2_write_or_panic(grown, "far away", 8, 524 * BLOCK);
    if (grown->size != 524 * BLOCK + 8)
        panic("ext2 self-test: grow/long did not grow to its new end\n");
    if (vfs_read(grown, back, 16, 524 * BLOCK - 8) != 16 || memcmp(back, "\0\0\0\0\0\0\0\0far away", 16) != 0)
        panic("ext2 self-test: grow/long's new end reads back wrong\n");
    if (vfs_read(grown, back, 100, BIG_SIZE - 50) != 100 || !big_line_ok(back + 42, BIG_LINES - 1))
        panic("ext2 self-test: grow/long's old last line changed\n");
    for (uint32_t b = 50; b < 100; b++)
        if (back[b] != 0)
            panic("ext2 self-test: grow/long's gap after its old end is not zeros\n");
    grow_expect(disk, grown, 606 + 4, 2, &free, &group0, "an append needing a new second-level table");
    vfs_node_put(grown);

    kfree(back);
    kfree(pattern);
}

/**
 * The mounted test disk is writable, so on the disk itself (after a sync) its
 * s_state says "not cleanly unmounted": the clean bit comes back only at the
 * halt (fsck-test.sh checks that). A crash before it leaves the bit clear.
 */
static void ext2_test_state(void)
{
    block_device_t *disk = block_at(0);
    if (disk == NULL)
        panic("ext2 self-test: no disk\n");
    if (block_cache_sync() != 0)
        panic("ext2 self-test: sync failed\n");

    uint32_t state = disk_u32(disk, SB + SB_STATE) & 0xFFFF;
    if (state & EXT2_TEST_VALID_FS)
        panic("ext2 self-test: the mounted volume's s_state (0x%x) still says clean\n", state);
}

/**
 * Truncating the grow/ files ext2_test_grow left behind: every block past the
 * new end and every table left empty go back to both free counts (checked on
 * the disk after each step), the tail of the new last block reads as zeros
 * when the file grows again, and truncating up leaves a hole. In the end the
 * four files hold what the test disk gave them, block for block: small 1,
 * holes 3, empty 0, and long none instead of its 303 (it ends empty).
 */
static void ext2_test_truncate(void)
{
    block_device_t *disk = block_at(0);
    char *back = kmalloc(2 * BLOCK);
    if (disk == NULL || back == NULL)
        panic("ext2 self-test: no disk, or out of memory\n");
    uint32_t group0;
    uint32_t free = disk_free_blocks(disk, &group0);

    // long (610: 300 data + the 524 append's block, single, double and two second-level tables)
    vfs_node_t *grown = ext2_expect(MNT "/grow/long", VFS_FILE);
    ext2_truncate_or_panic(grown, BIG_SIZE);
    grow_expect(disk, grown, 606, -2, &free, &group0, "truncating the 524 append away");
    if (vfs_read(grown, back, 16, BIG_SIZE - 8) != 8 || !big_line_ok(back, BIG_LINES - 1))
        panic("ext2 self-test: grow/long's last line changed in a truncate\n");

    // To 13 KiB + 100: 14 blocks stay (12 direct, 2 in the single table, the second one partly);
    // 254 single-indirect blocks go, and the whole double tree (32 blocks and 2 tables)
    ext2_truncate_or_panic(grown, 13 * BLOCK + 100);
    grow_expect(disk, grown, 30, -288, &free, &group0, "truncating into the single-indirect range");
    if (vfs_read(grown, back, 108, 13 * BLOCK - 4) != 104 || !big_line_ok(back + 4, (13 * BLOCK) / 8))
        panic("ext2 self-test: grow/long's kept bytes changed in a truncate\n");

    // Up again by truncate: a hole, no blocks; the cut block's tail and the hole read as zeros
    ext2_truncate_or_panic(grown, 14 * BLOCK + 50);
    grow_expect(disk, grown, 30, 0, &free, &group0, "truncating up");
    if (vfs_read(grown, back, 2 * BLOCK, 13 * BLOCK + 100) != BLOCK - 50)
        panic("ext2 self-test: grow/long does not read up to its new end\n");
    for (uint32_t b = 0; b < BLOCK - 50; b++)
        if (back[b] != 0)
            panic("ext2 self-test: byte %u of grow/long is not zero after truncating down and up\n",
                  13 * BLOCK + 100 + b);

    ext2_truncate_or_panic(grown, 0);
    grow_expect(disk, grown, 0, -15, &free, &group0, "truncating to 0");
    vfs_node_put(grown);

    // small (1105 bytes, 2 blocks) back to 5: then a write at 1100 needs a new block, and 5..1099 are zeros
    vfs_node_t *small = ext2_expect(MNT "/grow/small", VFS_FILE);
    ext2_truncate_or_panic(small, 5);
    grow_expect(disk, small, 2, -1, &free, &group0, "truncating small to 5");
    ext2_write_or_panic(small, "Z", 1, 1100);
    grow_expect(disk, small, 4, 1, &free, &group0, "growing small again");
    if (vfs_read(small, back, 1101, 0) != 1101 || memcmp(back, "small", 5) != 0 || back[1100] != 'Z')
        panic("ext2 self-test: grow/small reads back wrong after a truncate and a write\n");
    for (uint32_t b = 5; b < 1100; b++)
        if (back[b] != 0)
            panic("ext2 self-test: byte %u of grow/small came back after a truncate\n", b);
    ext2_truncate_or_panic(small, 5);
    grow_expect(disk, small, 2, -1, &free, &group0, "truncating small to 5 again");
    vfs_node_put(small);

    // holes (blocks 0, 1, 13, 16 and the table) to 1 KiB: only block 0 stays; then "end" at
    // 16384 as on the test disk (block 16 and a new table)
    vfs_node_t *holes = ext2_expect(MNT "/grow/holes", VFS_FILE);
    ext2_truncate_or_panic(holes, BLOCK);
    grow_expect(disk, holes, 2, -4, &free, &group0, "truncating holes to its first block");
    ext2_write_or_panic(holes, "end", 3, 16384);
    grow_expect(disk, holes, 6, 2, &free, &group0, "writing holes's end again");
    vfs_node_put(holes);

    vfs_node_t *empty = ext2_expect(MNT "/grow/empty", VFS_FILE);
    ext2_truncate_or_panic(empty, 0);
    grow_expect(disk, empty, 0, -14, &free, &group0, "truncating empty to 0");

    // Refusals: past the largest file, a directory, a disk
    if (vfs_truncate(empty, (uint64_t)1 << 62) != -EFBIG)
        panic("ext2 self-test: a truncate past the largest file did not return -EFBIG\n");
    vfs_node_put(empty);
    vfs_node_t *docs = ext2_expect(MNT "/docs", VFS_DIRECTORY);
    if (vfs_truncate(docs, 0) != -EISDIR)
        panic("ext2 self-test: truncating a directory did not return -EISDIR\n");
    vfs_node_put(docs);
    char path[sizeof("/dev/") + BLOCK_NAME_MAX];
    memcpy(path, "/dev/", 5);
    strlcpy(path + 5, disk->name, BLOCK_NAME_MAX);
    vfs_node_t *device = NULL;
    if (vfs_lookup(path, &device) != 0 || vfs_truncate(device, 0) != -EINVAL)
        panic("ext2 self-test: truncating %s did not return -EINVAL\n", path);
    vfs_node_put(device);

    // Everything ext2_test_grow took is back, and long's 303 blocks on top
    if (free != grow_free_before + 303)
        panic("ext2 self-test: %u blocks free after the truncates, expected %u\n", free, grow_free_before + 303);

    kfree(back);
}

// The free inode counts on the disk itself (after a sync): the superblock's and group 0's
static uint32_t disk_free_inodes(block_device_t *disk, uint32_t *group0)
{
    if (block_cache_sync() != 0)
        panic("ext2 self-test: sync failed\n");
    *group0 = disk_u32(disk, GROUP_TABLE + BG_FREE_INODES_COUNT) & 0xFFFF;
    return disk_u32(disk, SB + SB_FREE_INODES_COUNT);
}

// vfs_open of path returns `error`, or (0) a file that is put again at once
static void open_expect(const char *path, int flags, int error)
{
    struct file *file = NULL;
    int ret = vfs_open(path, flags, 0644, &file);
    if (ret != error)
        panic("ext2 self-test: opening '%s' (flags 0x%x) returned %d instead of %d\n", path, flags, ret, error);
    if (ret == 0)
        file_put(file);
}

/**
 * Creating files in grow/ (no listing test reads it): an O_CREAT | O_EXCL open
 * takes the first free inode (29 on the test disk) and prepares its slot (on the
 * disk: the mode, one link, the parent's mtime, no blocks, no i_dtime, i_extra_isize
 * 32 and zeros after it), its bitmap bit and one inode from both counts, no
 * directory counted; the name's record fits in the directory's first block. A
 * second O_EXCL open is -EEXIST, a plain O_CREAT one opens the same file. 90 more
 * names need more than the block: the directory gets a second one (its size and
 * i_blocks on the disk), every name looks up to its inode. Then the refusals,
 * from the path (EISDIR, ENOENT, ENOTDIR, ENAMETOOLONG), the file system (EROFS
 * on devfs, and on the initrd in the live boot) and the symlink's name (taken: ENOENT, EEXIST with
 * O_EXCL). e2fsck checks the result (fsck-test.sh).
 */
static uint32_t create_free_inodes, create_free_blocks; // before ext2_test_create, for ext2_test_unlink

static void ext2_test_create(void)
{
    block_device_t *disk = block_at(0);
    if (disk == NULL)
        panic("ext2 self-test: no disk\n");
    uint32_t inode_size = disk_u32(disk, SB + SB_INODE_SIZE) & 0xFFFF;
    uint32_t inode_table = disk_u32(disk, GROUP_TABLE + BG_INODE_TABLE);
    uint32_t inode_bitmap = disk_u32(disk, GROUP_TABLE + BG_INODE_BITMAP);
    uint32_t used_dirs = disk_u32(disk, GROUP_TABLE + BG_USED_DIRS_COUNT) & 0xFFFF;
    uint32_t group0, blocks_group0;
    uint32_t free = disk_free_inodes(disk, &group0);
    uint32_t free_blocks = disk_free_blocks(disk, &blocks_group0);
    create_free_inodes = free;
    create_free_blocks = free_blocks;

    vfs_node_t *grow = ext2_expect(MNT "/grow", VFS_DIRECTORY);
    if (grow->size != BLOCK)
        panic("ext2 self-test: grow/ is not one block before the creates\n");

    // A new file: the first free inode, prepared on the disk
    struct file *file = NULL;
    int ret = vfs_open(MNT "/grow/new", O_RDWR | O_CREAT | O_EXCL, 0640, &file);
    if (ret != 0)
        panic("ext2 self-test: creating grow/new failed: %d\n", ret);
    vfs_node_t *created = file->node;
    if (created->type != VFS_FILE || created->ino != 29 || created->size != 0 || created->mode != 0640 ||
        created->nlink != 1 || created->mtime != grow->mtime || created->dev != grow->dev)
        panic("ext2 self-test: grow/new is not an empty file in inode 29 with mode 0640\n");

    uint32_t now_group0;
    uint32_t now = disk_free_inodes(disk, &now_group0);
    if (free - now != 1 || group0 - now_group0 != 1)
        panic("ext2 self-test: creating a file took %u inodes from the superblock's count and %u from the group's\n",
              free - now, group0 - now_group0);
    if ((disk_u32(disk, (uint64_t)inode_bitmap * BLOCK + 28 / 8) >> (28 % 8) & 1) == 0)
        panic("ext2 self-test: inode 29's bitmap bit is not set\n");
    if ((disk_u32(disk, GROUP_TABLE + BG_USED_DIRS_COUNT) & 0xFFFF) != used_dirs)
        panic("ext2 self-test: creating a file counted a directory\n");

    uint8_t *slot = kmalloc(2 * BLOCK);
    if (slot == NULL)
        panic("ext2 self-test: out of memory\n");
    uint64_t byte = (uint64_t)inode_table * BLOCK + 28 * inode_size;
    if (block_read(disk, byte / BLOCK_SECTOR_SIZE, 2, slot) != 0)
        panic("ext2 self-test: reading inode 29 failed\n");
    uint8_t *inode = slot + byte % BLOCK_SECTOR_SIZE;
    uint16_t mode, links, extra;
    uint32_t mtime, dtime, size, blocks;
    memcpy(&mode, inode + I_MODE, 2);
    memcpy(&size, inode + I_SIZE, 4);
    memcpy(&mtime, inode + I_MTIME, 4);
    memcpy(&dtime, inode + I_DTIME, 4);
    memcpy(&links, inode + I_LINKS_COUNT, 2);
    memcpy(&blocks, inode + I_BLOCKS, 4);
    memcpy(&extra, inode + I_EXTRA_ISIZE, 2);
    if (mode != (0x8000 | 0640) || size != 0 || mtime != (uint32_t)grow->mtime || dtime != 0 || links != 1 ||
        blocks != 0)
        panic("ext2 self-test: inode 29 on the disk is not the new file\n");
    for (uint32_t b = I_BLOCK; b < 128; b++)
        if (inode[b] != 0 && (b < I_MTIME || b >= I_MTIME + 4))
            panic("ext2 self-test: byte %u of inode 29 is not zero\n", b);
    if (inode_size > 128)
    {
        if (extra != 32)
            panic("ext2 self-test: inode 29's i_extra_isize is %u, expected 32\n", extra);
        for (uint32_t b = I_EXTRA_ISIZE + 2; b < inode_size; b++)
            if (inode[b] != 0)
                panic("ext2 self-test: byte %u of inode 29's extra area is not zero\n", b);
    }
    kfree(slot);

    // It reads and writes like any file; the name is found again, and an O_EXCL open is refused
    if (vfs_write(created, "created", 7, 0) != 7)
        panic("ext2 self-test: writing grow/new failed\n");
    file_put(file);
    ext2_expect_text(MNT "/grow/new", "created");
    open_expect(MNT "/grow/new", O_WRONLY | O_CREAT | O_EXCL, -EEXIST);
    if (vfs_open(MNT "/grow/new", O_RDONLY | O_CREAT, 0, &file) != 0 || file->node->ino != 29 ||
        file->node->size != 7)
        panic("ext2 self-test: an O_CREAT open of an existing file did not open it\n");
    file_put(file);

    // The first block is full after these: the directory gets its second one
    uint32_t dir_blocks, dir_size;
    for (uint32_t i = 0; i < 90; i++)
    {
        char path[] = MNT "/grow/f000";
        path[sizeof(path) - 4] = (char)('0' + i / 100);
        path[sizeof(path) - 3] = (char)('0' + i / 10 % 10);
        path[sizeof(path) - 2] = (char)('0' + i % 10);
        open_expect(path, O_WRONLY | O_CREAT | O_EXCL, 0);
    }
    disk_inode_sizes(disk, grow, &dir_blocks, &dir_size);
    if (grow->size != 2 * BLOCK || dir_size != 2 * BLOCK || dir_blocks != 4)
        panic("ext2 self-test: grow/ did not get a second block (size %u, i_blocks %u)\n", dir_size, dir_blocks);
    for (uint32_t i = 0; i < 90; i++)
    {
        char path[] = MNT "/grow/f000";
        path[sizeof(path) - 4] = (char)('0' + i / 100);
        path[sizeof(path) - 3] = (char)('0' + i / 10 % 10);
        path[sizeof(path) - 2] = (char)('0' + i % 10);
        vfs_node_t *node = ext2_expect(path, VFS_FILE);
        if (node->ino != 30 + i || node->size != 0)
            panic("ext2 self-test: %s is not the empty inode %u\n", path, 30 + i);
        vfs_node_put(node);
    }
    now = disk_free_inodes(disk, &now_group0);
    if (free - now != 91 || group0 - now_group0 != 91)
        panic("ext2 self-test: 91 creates took %u inodes from the superblock's count and %u from the group's\n",
              free - now, group0 - now_group0);
    uint32_t now_blocks = disk_free_blocks(disk, &now_group0);
    if (free_blocks - now_blocks != 2 || blocks_group0 - now_group0 != 2)
        panic("ext2 self-test: the creates took %u blocks, expected 2 (grow/new's data and grow/'s second block)\n",
              free_blocks - now_blocks);
    vfs_node_put(grow);

    // Refusals
    open_expect(MNT "/grow/dir/", O_WRONLY | O_CREAT, -EISDIR);
    open_expect(MNT "/nope/new", O_WRONLY | O_CREAT, -ENOENT);
    open_expect(MNT "/hello.txt/new", O_WRONLY | O_CREAT, -ENOTDIR);
    if (!root_on_disk)
        open_expect("/usr/new", O_WRONLY | O_CREAT, -EROFS);
    open_expect("/dev/new", O_WRONLY | O_CREAT, -EROFS);
    open_expect(MNT "/link", O_WRONLY | O_CREAT, -ENOENT);
    open_expect(MNT "/link", O_WRONLY | O_CREAT | O_EXCL, -EEXIST);
    open_expect(MNT "/docs", O_RDONLY | O_CREAT, 0);
    open_expect(MNT "/docs", O_WRONLY | O_CREAT, -EISDIR);
    char too_long[sizeof(MNT "/grow/") + LONG_NAME_LENGTH + 1];
    memcpy(too_long, MNT "/grow/", sizeof(MNT "/grow/") - 1);
    memset(too_long + sizeof(MNT "/grow/") - 1, 'b', LONG_NAME_LENGTH + 1);
    too_long[sizeof(too_long) - 1] = '\0';
    open_expect(too_long, O_WRONLY | O_CREAT, -ENAMETOOLONG);
    now = disk_free_inodes(disk, &now_group0);
    if (free - now != 91)
        panic("ext2 self-test: a refused create took an inode\n");
}

// grow/'s file names (f000…) for the create and unlink tests
static void grow_name(char *path, uint32_t i)
{
    memcpy(path, MNT "/grow/f000", sizeof(MNT "/grow/f000"));
    path[sizeof(MNT "/grow/f000") - 4] = (char)('0' + i / 100);
    path[sizeof(MNT "/grow/f000") - 3] = (char)('0' + i / 10 % 10);
    path[sizeof(MNT "/grow/f000") - 2] = (char)('0' + i % 10);
}

static void unlink_expect(const char *path, int error)
{
    int ret = vfs_unlink(path);
    if (ret != error)
        panic("ext2 self-test: unlinking '%s' returned %d instead of %d\n", path, ret, error);
}

/**
 * Unlinking what ext2_test_create made. grow/new is unlinked while open: the
 * name is gone at once (lookup ENOENT, nlink 0, i_links_count 0 on the disk),
 * the file still reads and writes, and its inode and block come back only at
 * the last put (bit 28 clear, i_dtime the superblock's s_wtime: no clock); a new create then reuses inode 29,
 * and unlinking a closed file frees it at once. The 90 f… names go too: in
 * the end grow/ lists its 5 files again, and the free counts are what they were
 * before the creates, but for grow/'s second block (directories never shrink).
 * Then the refusals: EISDIR (a directory, with or without '/', the dots, "/"),
 * ENOTDIR, ENOENT (missing, the symlink), EROFS (devfs; the initrd in the live boot).
 */
static void ext2_test_unlink(void)
{
    block_device_t *disk = block_at(0);
    if (disk == NULL)
        panic("ext2 self-test: no disk\n");
    uint32_t inode_size = disk_u32(disk, SB + SB_INODE_SIZE) & 0xFFFF;
    uint64_t inode29 = (uint64_t)disk_u32(disk, GROUP_TABLE + BG_INODE_TABLE) * BLOCK + 28 * inode_size;
    uint64_t bitmap = (uint64_t)disk_u32(disk, GROUP_TABLE + BG_INODE_BITMAP) * BLOCK;
    uint32_t group0, blocks_group0, now_group0;
    uint32_t free = disk_free_inodes(disk, &group0);
    uint32_t free_blocks = disk_free_blocks(disk, &blocks_group0);

    // Open, unlinked, still usable; nothing freed while it is open
    struct file *file = NULL;
    if (vfs_open(MNT "/grow/new", O_RDWR, 0, &file) != 0)
        panic("ext2 self-test: opening grow/new failed\n");
    vfs_node_t *node = file->node;
    unlink_expect(MNT "/grow/new", 0);
    ext2_expect_error(MNT "/grow/new", -ENOENT);
    if (node->nlink != 0 || (disk_free_inodes(disk, &now_group0), (disk_u32(disk, inode29 + I_LINKS_COUNT) & 0xFFFF) != 0))
        panic("ext2 self-test: grow/new still has links after its unlink\n");
    char text[16];
    if (vfs_write(node, " still", 6, 7) != 6 || vfs_read(node, text, sizeof(text), 0) != 13 ||
        memcmp(text, "created still", 13) != 0)
        panic("ext2 self-test: an unlinked open file does not read and write\n");
    if (disk_free_inodes(disk, &now_group0) != free || disk_free_blocks(disk, &now_group0) != free_blocks)
        panic("ext2 self-test: an unlinked file was freed while still open\n");

    // The last put frees it: its block and inode back, the slot marked deleted
    file_put(file);
    uint32_t now = disk_free_inodes(disk, &now_group0);
    if (now != free + 1 || now_group0 != group0 + 1)
        panic("ext2 self-test: closing an unlinked file gave back %u inodes, expected 1\n", now - free);
    if (disk_free_blocks(disk, &now_group0) != free_blocks + 1 || now_group0 != blocks_group0 + 1)
        panic("ext2 self-test: closing an unlinked file did not give back its block\n");
    if ((disk_u32(disk, bitmap + 28 / 8) >> (28 % 8) & 1) != 0 || disk_u32(disk, inode29 + I_DTIME) != disk_u32(disk, SB + SB_WTIME))
        panic("ext2 self-test: inode 29 is still marked used, or its i_dtime is not s_wtime, after its free\n");

    // The freed inode is the first free one again; unlinking a closed file frees it at once
    if (vfs_open(MNT "/grow/again", O_WRONLY | O_CREAT | O_EXCL, 0600, &file) != 0 || file->node->ino != 29)
        panic("ext2 self-test: a create after the free did not reuse inode 29\n");
    file_put(file);
    unlink_expect(MNT "/grow/again", 0);
    if (disk_free_inodes(disk, &now_group0) != free + 1)
        panic("ext2 self-test: unlinking a closed file did not free it at once\n");

    // The 90 empty files: inodes only
    char path[sizeof(MNT "/grow/f000")];
    for (uint32_t i = 0; i < 90; i++)
    {
        grow_name(path, i);
        unlink_expect(path, 0);
        ext2_expect_error(path, -ENOENT);
    }
    now = disk_free_inodes(disk, &now_group0);
    if (now != free + 91 || now_group0 != group0 + 91)
        panic("ext2 self-test: %u inodes came back from the unlinks, expected 91\n", now - free);
    if (now != create_free_inodes || disk_free_blocks(disk, &now_group0) != create_free_blocks - 1)
        panic("ext2 self-test: the free counts after the unlinks are not those before the creates\n");

    // grow/ lists what the test disk gave it (the dots and 5 files), its records merged back
    vfs_node_t *grow = ext2_expect(MNT "/grow", VFS_DIRECTORY);
    vfs_dirent_t entry;
    uint64_t count = 0;
    while (vfs_readdir(grow, count, &entry) == 1)
        count++;
    if (count != 7)
        panic("ext2 self-test: grow/ lists %u entries after the unlinks, expected 7\n", (unsigned int)count);
    vfs_node_put(grow);

    // Refusals
    unlink_expect(MNT "/docs", -EISDIR);
    unlink_expect(MNT "/docs/", -EISDIR);
    unlink_expect(MNT "/docs/.", -EISDIR);
    unlink_expect(MNT "/docs/..", -EISDIR);
    unlink_expect("/", -EISDIR);
    unlink_expect(MNT "/hello.txt/", -ENOTDIR);
    unlink_expect(MNT "/nope", -ENOENT);
    unlink_expect(MNT "/link", -ENOENT);
    if (!root_on_disk) // on the disk's root it would delete init
        unlink_expect("/sbin/init", -EROFS);
    unlink_expect("/dev/null", -EROFS); // devfs: read-only in both boots
    if (disk_free_inodes(disk, &now_group0) != create_free_inodes)
        panic("ext2 self-test: a refused unlink changed the free inodes\n");
}

// The used-directories count of group 0, on the disk itself (after a sync)
static uint32_t disk_used_dirs(block_device_t *disk)
{
    if (block_cache_sync() != 0)
        panic("ext2 self-test: sync failed\n");
    return disk_u32(disk, GROUP_TABLE + BG_USED_DIRS_COUNT) & 0xFFFF;
}

// An inode's 16-bit field at `field`, on the disk itself (after a sync)
static uint32_t disk_inode_u16(block_device_t *disk, uint32_t ino, uint32_t field)
{
    if (block_cache_sync() != 0)
        panic("ext2 self-test: sync failed\n");
    uint32_t inode_size = disk_u32(disk, SB + SB_INODE_SIZE) & 0xFFFF;
    uint64_t inode = (uint64_t)disk_u32(disk, GROUP_TABLE + BG_INODE_TABLE) * BLOCK + (ino - 1) * inode_size;
    return disk_u32(disk, inode + field) & 0xFFFF;
}

static void mkdir_expect(const char *path, int error)
{
    int ret = vfs_mkdir(path, 0755);
    if (ret != error)
        panic("ext2 self-test: mkdir of '%s' returned %d instead of %d\n", path, ret, error);
}

static void rmdir_expect(const char *path, int error)
{
    int ret = vfs_rmdir(path);
    if (ret != error)
        panic("ext2 self-test: rmdir of '%s' returned %d instead of %d\n", path, ret, error);
}

// A directory lists exactly "." (itself) and ".." (its parent)
static void dir_expect_empty(vfs_node_t *dir, uint64_t parent)
{
    vfs_dirent_t entry;
    if (vfs_readdir(dir, 0, &entry) != 1 || strcmp(entry.name, ".") != 0 || entry.ino != dir->ino ||
        vfs_readdir(dir, 1, &entry) != 1 || strcmp(entry.name, "..") != 0 || entry.ino != parent ||
        vfs_readdir(dir, 2, &entry) != 0)
        panic("ext2 self-test: directory %u does not hold just . and ..\n", (unsigned int)dir->ino);
}

/**
 * Directories, in grow/ (inode 20, 2 links). mkdir of grow/sub takes inode 29 as a
 * directory: mode 0755, 2 links, one block holding "." and ".." (i_blocks 2), both on
 * the disk; grow/ gets a third link (sub's ".."), group 0 one more used directory, one
 * inode and one block less free; sub/.. leads back. A trailing '/' works for both
 * calls. Nested: sub/inner with a file in it; rmdir is ENOTEMPTY for both until the
 * file and inner go. A directory removed while open: its name gone, nlink 0, it still
 * lists . and .., and takes no new file or directory (ENOENT); the close frees it. In
 * the end every count is what it was. Then the refusals of both calls.
 */
static void ext2_test_dirs(void)
{
    block_device_t *disk = block_at(0);
    if (disk == NULL)
        panic("ext2 self-test: no disk\n");
    uint32_t group0, blocks_group0, now_group0;
    uint32_t free = disk_free_inodes(disk, &group0);
    uint32_t free_blocks = disk_free_blocks(disk, &blocks_group0);
    uint32_t used_dirs = disk_used_dirs(disk);
    vfs_node_t *grow = ext2_expect(MNT "/grow", VFS_DIRECTORY);
    if (grow->ino != 20 || grow->nlink != 2)
        panic("ext2 self-test: grow/ is not inode 20 with 2 links\n");

    // A new directory, on the disk and in its node
    mkdir_expect(MNT "/grow/sub", 0);
    vfs_node_t *sub = ext2_expect(MNT "/grow/sub", VFS_DIRECTORY);
    if (sub->ino != 29 || sub->mode != 0755 || sub->nlink != 2 || sub->size != BLOCK || sub->parent != grow)
        panic("ext2 self-test: grow/sub is not an empty directory in inode 29\n");
    dir_expect_empty(sub, 20);
    if (disk_inode_u16(disk, 29, I_MODE) != (0x4000 | 0755) || disk_inode_u16(disk, 29, I_LINKS_COUNT) != 2 ||
        disk_inode_u16(disk, 29, I_BLOCKS) != 2)
        panic("ext2 self-test: inode 29 on the disk is not the new directory\n");
    if (grow->nlink != 3 || disk_inode_u16(disk, 20, I_LINKS_COUNT) != 3)
        panic("ext2 self-test: grow/ did not get the link of sub's ..\n");
    if (disk_used_dirs(disk) != used_dirs + 1 || disk_free_inodes(disk, &now_group0) != free - 1 ||
        disk_free_blocks(disk, &now_group0) != free_blocks - 1)
        panic("ext2 self-test: mkdir did not take one directory inode and one block\n");
    vfs_node_t *up = ext2_expect(MNT "/grow/sub/..", VFS_DIRECTORY);
    if (up != grow)
        panic("ext2 self-test: grow/sub/.. is not grow/\n");
    vfs_node_put(up);

    // A trailing '/' names a directory: fine for both
    mkdir_expect(MNT "/grow/slash/", 0);
    rmdir_expect(MNT "/grow/slash//", 0);

    // Nested, with a file: neither goes while it holds something
    mkdir_expect(MNT "/grow/sub/inner", 0);
    struct file *file = NULL;
    if (vfs_open(MNT "/grow/sub/inner/file", O_WRONLY | O_CREAT | O_EXCL, 0644, &file) != 0)
        panic("ext2 self-test: creating a file in a new directory failed\n");
    file_put(file);
    if (sub->nlink != 3)
        panic("ext2 self-test: grow/sub did not get the link of inner's ..\n");
    rmdir_expect(MNT "/grow/sub", -ENOTEMPTY);
    rmdir_expect(MNT "/grow/sub/inner", -ENOTEMPTY);
    unlink_expect(MNT "/grow/sub/inner/file", 0);
    rmdir_expect(MNT "/grow/sub/inner", 0);
    ext2_expect_error(MNT "/grow/sub/inner", -ENOENT);
    if (sub->nlink != 2 || disk_inode_u16(disk, 29, I_LINKS_COUNT) != 2)
        panic("ext2 self-test: grow/sub kept the link of a removed directory\n");

    // Removed while open: still listed by its holder, no new names, freed at the close
    if (vfs_open(MNT "/grow/sub", O_RDONLY | O_DIRECTORY, 0, &file) != 0)
        panic("ext2 self-test: opening grow/sub failed\n");
    vfs_node_put(sub);
    sub = file->node;
    rmdir_expect(MNT "/grow/sub", 0);
    ext2_expect_error(MNT "/grow/sub", -ENOENT);
    if (sub->nlink != 0 || grow->nlink != 2 || disk_inode_u16(disk, 20, I_LINKS_COUNT) != 2)
        panic("ext2 self-test: rmdir left links on grow/sub or grow/\n");
    dir_expect_empty(sub, 20);
    vfs_node_t *child = NULL;
    if (sub->ops->create(sub, "x", 1, 0644, &child) != -ENOENT || sub->ops->mkdir(sub, "x", 1, 0755) != -ENOENT ||
        child != NULL)
        panic("ext2 self-test: a removed directory took a new name\n");
    if (disk_used_dirs(disk) != used_dirs + 1)
        panic("ext2 self-test: a removed directory was freed while still open\n");
    file_put(file);
    if (disk_used_dirs(disk) != used_dirs || disk_free_inodes(disk, &now_group0) != free ||
        now_group0 != group0 || disk_free_blocks(disk, &now_group0) != free_blocks || now_group0 != blocks_group0)
        panic("ext2 self-test: the counts are not back after the rmdirs\n");
    vfs_node_put(grow);

    // Refusals
    mkdir_expect(MNT "/grow/small", -EEXIST);
    mkdir_expect(MNT "/docs", -EEXIST);
    mkdir_expect(MNT "/link", -EEXIST); // a symlink: lookup cannot see it, but the name is taken
    mkdir_expect("/", -EEXIST);
    if (!root_on_disk)
        mkdir_expect("/usr/new", -EROFS);
    mkdir_expect("/dev/new", -EROFS); // devfs: read-only in both boots
    mkdir_expect(MNT "/nope/new", -ENOENT);
    mkdir_expect(MNT "/hello.txt/new", -ENOTDIR);
    rmdir_expect(MNT "/hello.txt", -ENOTDIR);
    rmdir_expect(MNT "/nope", -ENOENT);
    rmdir_expect(MNT "/link", -ENOENT);
    rmdir_expect(MNT "/docs", -ENOTEMPTY);
    rmdir_expect(MNT "/docs/.", -EINVAL);
    rmdir_expect(MNT "/docs/..", -ENOTEMPTY);
    rmdir_expect("/", -EBUSY);
    rmdir_expect("/usr", root_on_disk ? -ENOTEMPTY : -EROFS);

    // A mount point: on the initrd its parent cannot change at all (EROFS first, as in Linux);
    // on the disk's writable root the directory is busy, though empty on the disk itself
    rmdir_expect(MNT, root_on_disk ? -EBUSY : -EROFS);
    rmdir_expect("/dev", root_on_disk ? -EBUSY : -EROFS);
    if (disk_used_dirs(disk) != used_dirs || disk_free_inodes(disk, &now_group0) != free)
        panic("ext2 self-test: a refused mkdir or rmdir changed the counts\n");
}

#define FULL_CHUNK 4096 // the full-disk test writes this much per call

// Chunk `chunk` of the full-disk test's file: its number in the first 4 bytes, then a pattern from it
static void full_chunk(uint8_t *buffer, uint32_t chunk)
{
    memcpy(buffer, &chunk, sizeof(chunk));
    for (uint32_t b = sizeof(chunk); b < FULL_CHUNK; b++)
        buffer[b] = (uint8_t)(chunk * 7 + b);
}

/**
 * The disk filled to the last block, through grow/full, 4 KiB per write: the writes end
 * cleanly (a short write, or -ENOSPC when not one byte fits), the size is what was
 * written, both free counts are 0 on the disk, and one more byte is -ENOSPC. Full, a
 * create still works (it needs an inode and room in grow/'s blocks, no new block), but
 * writing to it is -ENOSPC; a mkdir (its first block) is -ENOSPC and gives back the inode
 * and the used-directories count it took. Every chunk reads back (its number; every
 * 64th whole). Unlinking both files brings back every block and inode. e2fsck checks
 * the disk afterwards (fsck-test.sh).
 */
static void ext2_test_full(void)
{
    block_device_t *disk = block_at(0);
    uint8_t *buffer = kmalloc(FULL_CHUNK);
    uint8_t *back = kmalloc(FULL_CHUNK);
    if (disk == NULL || buffer == NULL || back == NULL)
        panic("ext2 self-test: no disk, or out of memory\n");
    uint32_t group0, blocks_group0, now_group0;
    uint32_t free = disk_free_inodes(disk, &group0);
    uint32_t free_blocks = disk_free_blocks(disk, &blocks_group0);
    uint32_t used_dirs = disk_used_dirs(disk);

    struct file *full_file = NULL;
    if (vfs_open(MNT "/grow/full", O_RDWR | O_CREAT | O_EXCL, 0644, &full_file) != 0)
        panic("ext2 self-test: creating grow/full failed\n");
    vfs_node_t *full = full_file->node;

    // Fill: every write a whole chunk until the disk runs out
    uint64_t written = 0;
    uint32_t chunks = 0;
    for (;;)
    {
        full_chunk(buffer, chunks);
        long n = vfs_write(full, buffer, FULL_CHUNK, written);
        if (n == -ENOSPC)
            break;
        if (n <= 0 || n > FULL_CHUNK)
            panic("ext2 self-test: a write filling the disk returned %d\n", (int)n);
        written += (uint64_t)n;
        chunks++;
        if (n < FULL_CHUNK)
            break; // the disk filled inside this chunk
        if (written > (uint64_t)free_blocks * BLOCK)
            panic("ext2 self-test: more was written than the disk had free\n");
    }
    if (full->size != written || written == 0 || written % BLOCK != 0)
        panic("ext2 self-test: the full file's size is %u, %u bytes were written\n", (unsigned int)full->size,
              (unsigned int)written);
    if (disk_free_blocks(disk, &now_group0) != 0 || now_group0 != 0)
        panic("ext2 self-test: the disk is full but its free counts are not 0\n");
    if (vfs_write(full, "x", 1, written) != -ENOSPC || full->size != written)
        panic("ext2 self-test: a byte past a full disk's last block did not return -ENOSPC\n");

    // Full: a create takes no block, a write to it does; a mkdir needs its first block
    struct file *file = NULL;
    if (vfs_open(MNT "/grow/nothing", O_RDWR | O_CREAT | O_EXCL, 0644, &file) != 0)
        panic("ext2 self-test: creating a file on a full disk failed\n");
    if (vfs_write(file->node, "x", 1, 0) != -ENOSPC || file->node->size != 0)
        panic("ext2 self-test: writing to a new file on a full disk did not return -ENOSPC\n");
    file_put(file);
    uint32_t inodes_full = disk_free_inodes(disk, &now_group0);
    mkdir_expect(MNT "/grow/nospace", -ENOSPC);
    ext2_expect_error(MNT "/grow/nospace", -ENOENT);
    if (disk_free_inodes(disk, &now_group0) != inodes_full || disk_used_dirs(disk) != used_dirs)
        panic("ext2 self-test: a mkdir refused for space kept its inode\n");

    // Every chunk is there: its number, and every 64th byte for byte
    for (uint32_t chunk = 0; chunk < chunks; chunk++)
    {
        uint64_t at = (uint64_t)chunk * FULL_CHUNK;
        size_t length = (size_t)MIN((uint64_t)FULL_CHUNK, written - at);
        bool whole = chunk % 64 == 0 || chunk == chunks - 1;
        if (vfs_read(full, back, whole ? length : sizeof(uint32_t), at) != (long)(whole ? length : sizeof(uint32_t)))
            panic("ext2 self-test: reading chunk %u of the full file failed\n", chunk);
        full_chunk(buffer, chunk);
        if (memcmp(back, buffer, whole ? length : sizeof(uint32_t)) != 0)
            panic("ext2 self-test: chunk %u of the full file reads back wrong\n", chunk);
    }

    // Everything back: the full file unlinked while open keeps its blocks until the close
    unlink_expect(MNT "/grow/nothing", 0);
    unlink_expect(MNT "/grow/full", 0);
    if (disk_free_blocks(disk, &now_group0) != 0)
        panic("ext2 self-test: the unlinked full file gave blocks back while still open\n");
    file_put(full_file);
    if (disk_free_blocks(disk, &now_group0) != free_blocks || now_group0 != blocks_group0)
        panic("ext2 self-test: closing the full file did not give all its blocks back\n");
    if (disk_free_inodes(disk, &now_group0) != free || now_group0 != group0)
        panic("ext2 self-test: the full-disk test did not give its inodes back\n");
    kfree(back);
    kfree(buffer);
}

void ext2_selftest(const block_device_t *root_device)
{
    root_on_disk = root_device != NULL;

    memcpy(long_path, MNT "/", sizeof(MNT));
    memset(long_path + sizeof(MNT), 'a', LONG_NAME_LENGTH);
    long_path[sizeof(MNT) + LONG_NAME_LENGTH] = '\0';

    vfs_node_t *mnt = ext2_expect(MNT, VFS_DIRECTORY);
    unsigned int refs = mnt->refs;

    ext2_test_root();
    ext2_test_lookup();
    ext2_test_nodes();
    ext2_test_read();
    ext2_test_write();
    ext2_test_grow();
    ext2_test_truncate();
    ext2_test_create();
    ext2_test_unlink();
    ext2_test_dirs();
    ext2_test_full();
    ext2_test_state();
    ext2_test_broken();

    if (mnt->refs != refs)
        panic("ext2 self-test: /mnt's references leaked (%u, was %u)\n", mnt->refs, refs);
    vfs_node_put(mnt);
}
