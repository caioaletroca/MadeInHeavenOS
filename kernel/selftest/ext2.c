#include <selftest/ext2.h>
#include <fs/ext2.h>
#include <fs/vfs.h>
#include <fs/file.h>
#include <driver/block.h>
#include <mm/kmalloc.h>
#include <mihos/errno.h>
#include <mihos/fcntl.h>
#include <mihos/stat.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <panic.h>

/*
 * Against disk.img (Makefile): 1 KiB blocks, mounted on /mnt by ext2_init.
 * The layout comes from what mke2fs writes; offsets of on-disk fields are the
 * ext2 spec's, written out here on purpose (not taken from ext2_disk.h).
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
#define GROUP_TABLE (2 * BLOCK) // block 2 with 1 KiB blocks
#define BG_INODE_TABLE 8
#define I_MODE 0
#define I_BLOCK 40
#define DIRENT_REC_LEN 4
#define DIRENT_NAME_LEN 6

static char long_path[sizeof(MNT "/") + LONG_NAME_LENGTH + 1];

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
        {"big.txt", VFS_FILE}, {"docs", VFS_DIRECTORY}, {"empty", VFS_FILE},  {"hello.txt", VFS_FILE},
        {"sparse", VFS_FILE},
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
    if (vfs_open(MNT "/hello.txt", O_RDWR, &file) != 0 || file->flags != (FILE_READ | FILE_WRITE))
        panic("ext2 self-test: opening hello.txt for reading and writing failed\n");
    file_put(file);
    if (vfs_open(MNT "/docs", O_RDONLY | O_DIRECTORY, &file) != 0)
        panic("ext2 self-test: opening /mnt/docs as a directory failed\n");
    if (vfs_read(file->node, &byte, 1, 0) != -EISDIR)
        panic("ext2 self-test: reading a directory did not return -EISDIR\n");
    file_put(file);
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

#define OVERLAYS 16
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
    uint8_t sector[BLOCK_SECTOR_SIZE];
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

    // Unchanged: an overlay mounts and lists like the real thing
    vfs_node_t *root;
    if (ext2_mount(overlay_create(disk, 0, 0, 0), &root) != 0 || root->ino != 2)
        panic("ext2 self-test: an unchanged overlay does not mount\n");
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

void ext2_selftest(void)
{
    memcpy(long_path, MNT "/", sizeof(MNT));
    memset(long_path + sizeof(MNT), 'a', LONG_NAME_LENGTH);
    long_path[sizeof(MNT) + LONG_NAME_LENGTH] = '\0';

    vfs_node_t *mnt = ext2_expect(MNT, VFS_DIRECTORY);
    unsigned int refs = mnt->refs;

    ext2_test_root();
    ext2_test_lookup();
    ext2_test_nodes();
    ext2_test_read();
    ext2_test_broken();

    if (mnt->refs != refs)
        panic("ext2 self-test: /mnt's references leaked (%u, was %u)\n", mnt->refs, refs);
    vfs_node_put(mnt);
}
