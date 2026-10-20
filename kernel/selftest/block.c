#include <selftest/block.h>
#include <driver/block.h>
#include <fs/vfs.h>
#include <fs/file.h>
#include <mm/kmalloc.h>
#include <mihos/errno.h>
#include <mihos/fcntl.h>
#include <mihos/stat.h>
#include <string.h>
#include <stdint.h>
#include <panic.h>

// disk.img (Makefile): 4 MiB of ext2 with 1 KiB blocks, labelled "mihos-test"
#define DISK_SECTORS (4 * 1024 * 1024 / BLOCK_SECTOR_SIZE)
#define DISK_LABEL "mihos-test"

// The ext2 superblock: always at byte 1024 of the disk, i.e. sector 2
#define SUPERBLOCK_SECTOR 2
#define EXT2_MAGIC 0xEF53
#define SB_LOG_BLOCK_SIZE 24 // block size = 1024 << this
#define SB_MAGIC 56
#define SB_VOLUME_NAME 120

// More than one READ SECTORS command (at most 256 sectors each)
#define LONG_READ 300

static uint16_t le16(const uint8_t *p)
{
    return (uint16_t)(p[0] | (p[1] << 8));
}

static uint32_t le32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

/**
 * The test disk: the first ATA slot that has one (hda on QEMU, hdb on Bochs,
 * whose CD is the primary master).
 */
static block_device_t *block_test_disk(void)
{
    static const char *const names[] = {"hda", "hdb", "hdc", "hdd"};

    for (size_t i = 0; i < sizeof(names) / sizeof(names[0]); i++)
    {
        block_device_t *dev = block_find(names[i]);
        if (dev != NULL)
            return dev;
    }

    panic("Block self-test: no disk (is disk.img attached?)\n");
}

static void block_read_or_panic(block_device_t *dev, uint64_t sector, uint32_t count, void *buffer)
{
    int ret = block_read(dev, sector, count, buffer);
    if (ret != 0)
        panic("Block self-test: reading %u sectors at %u failed: %d\n", (unsigned int)count, (unsigned int)sector,
              ret);
}

static void block_test_superblock(block_device_t *dev, uint8_t *sector)
{
    block_read_or_panic(dev, SUPERBLOCK_SECTOR, 1, sector);

    if (le16(sector + SB_MAGIC) != EXT2_MAGIC)
        panic("Block self-test: no ext2 magic in sector 2 (0x%x)\n", le16(sector + SB_MAGIC));
    if (le32(sector + SB_LOG_BLOCK_SIZE) != 0)
        panic("Block self-test: the test disk's blocks are not 1 KiB\n");
    if (memcmp(sector + SB_VOLUME_NAME, DISK_LABEL, sizeof(DISK_LABEL)) != 0)
        panic("Block self-test: the volume label is not '" DISK_LABEL "'\n");
}

static void block_test_reads(block_device_t *dev)
{
    uint8_t *one = kmalloc(BLOCK_SECTOR_SIZE);
    uint8_t *many = kmalloc(LONG_READ * BLOCK_SECTOR_SIZE);
    if (one == NULL || many == NULL)
        panic("Block self-test: out of memory\n");

    if (dev->sector_count != DISK_SECTORS)
        panic("Block self-test: %s has %u sectors, disk.img %u\n", dev->name, (unsigned int)dev->sector_count,
              DISK_SECTORS);

    block_test_superblock(dev, one);

    // Several sectors in one call are the single sectors one after the other
    block_read_or_panic(dev, 2, 2, many);
    block_read_or_panic(dev, 2, 1, one);
    if (memcmp(many, one, BLOCK_SECTOR_SIZE) != 0)
        panic("Block self-test: a two-sector read differs from sector 2\n");
    block_read_or_panic(dev, 3, 1, one);
    if (memcmp(many + BLOCK_SECTOR_SIZE, one, BLOCK_SECTOR_SIZE) != 0)
        panic("Block self-test: a two-sector read differs from sector 3\n");

    // A read longer than one command: the second command starts where the first stopped
    block_read_or_panic(dev, 0, LONG_READ, many);
    static const uint32_t probes[] = {0, 2, 255, 256, 257, LONG_READ - 1};
    for (size_t i = 0; i < sizeof(probes) / sizeof(probes[0]); i++)
    {
        block_read_or_panic(dev, probes[i], 1, one);
        if (memcmp(many + probes[i] * BLOCK_SECTOR_SIZE, one, BLOCK_SECTOR_SIZE) != 0)
            panic("Block self-test: sector %u of a %u-sector read differs\n", probes[i], LONG_READ);
    }

    // Nothing to read: 0, and the buffer is untouched
    memset(one, 0xA5, BLOCK_SECTOR_SIZE);
    if (block_read(dev, 0, 0, one) != 0 || one[0] != 0xA5 || one[BLOCK_SECTOR_SIZE - 1] != 0xA5)
        panic("Block self-test: a 0-sector read did something\n");

    // The last sector is readable, nothing after it; ranges that wrap are refused
    uint64_t last = dev->sector_count - 1;
    block_read_or_panic(dev, last, 1, one);
    if (block_read(dev, dev->sector_count, 1, one) != -EINVAL ||
        block_read(dev, last, 2, one) != -EINVAL ||
        block_read(dev, UINT64_MAX, 2, one) != -EINVAL ||
        block_read(dev, 1, UINT32_MAX, one) != -EINVAL)
        panic("Block self-test: a read past the end of the disk was not refused\n");

    kfree(many);
    kfree(one);
}

/**
 * The disk as /dev/<name>: a block device node, read at byte offsets. Every
 * read must give the same bytes as whole-sector reads of the same range.
 */
static void block_test_node(block_device_t *dev)
{
    char path[sizeof("/dev/") + BLOCK_NAME_MAX];
    memcpy(path, "/dev/", 5);
    strlcpy(path + 5, dev->name, BLOCK_NAME_MAX);

    vfs_node_t *node;
    int ret = vfs_lookup(path, &node);
    if (ret != 0)
        panic("Block self-test: lookup of %s failed: %d\n", path, ret);
    unsigned int refs = node->refs;

    uint64_t size = dev->sector_count * BLOCK_SECTOR_SIZE;
    if (node->type != VFS_BLOCKDEV || node->rdev != dev->rdev || node->size != size)
        panic("Block self-test: %s is not a block device of %s's size and number\n", path, dev->name);
    if (MIHOS_MAJOR(dev->rdev) != 3 && MIHOS_MAJOR(dev->rdev) != 22)
        panic("Block self-test: %s has major %u, not an IDE one\n", dev->name, MIHOS_MAJOR(dev->rdev));

    // stat: like Linux, a device reports no size (lseek to the end finds it)
    struct mihos_stat st;
    vfs_stat(node, &st);
    if (st.st_mode != (MIHOS_S_IFBLK | 0660) || st.st_rdev != dev->rdev || st.st_size != 0 || st.st_blocks != 0 ||
        st.st_dev != node->dev)
        panic("Block self-test: stat of %s is wrong (mode 0%o)\n", path, st.st_mode);

    // Opens read-only through the VFS (offsets, lseek); write access is refused
    struct file *file;
    ret = vfs_open(path, O_RDONLY, &file);
    if (ret != 0)
        panic("Block self-test: opening %s failed: %d\n", path, ret);
    if (file->node != node || file->flags != FILE_READ)
        panic("Block self-test: %s opened with the wrong node or flags\n", path);
    file_put(file);
    if (vfs_open(path, O_RDWR, &file) != -EROFS || vfs_open(path, O_WRONLY, &file) != -EROFS)
        panic("Block self-test: opening %s for writing did not return -EROFS\n", path);

    uint8_t *sectors = kmalloc(LONG_READ * BLOCK_SECTOR_SIZE);
    uint8_t *bytes = kmalloc(LONG_READ * BLOCK_SECTOR_SIZE + 1);
    if (sectors == NULL || bytes == NULL)
        panic("Block self-test: out of memory\n");
    block_read_or_panic(dev, 0, LONG_READ, sectors);

    static const struct
    {
        uint64_t offset;
        size_t count;
    } cases[] = {
        {0, BLOCK_SECTOR_SIZE},                               // one whole sector
        {1024 + SB_MAGIC, 2},                                 // inside one sector: the ext2 magic
        {1000, 100},                                          // across sectors 1 and 2
        {510, 2 * BLOCK_SECTOR_SIZE + 20},                    // a head, two whole sectors, a tail
        {BLOCK_SECTOR_SIZE, 270 * BLOCK_SECTOR_SIZE},         // whole sectors, two commands
        {3, LONG_READ * BLOCK_SECTOR_SIZE - 3},               // everything after an unaligned start
        {255 * BLOCK_SECTOR_SIZE + 7, 2 * BLOCK_SECTOR_SIZE}, // around a command boundary
    };
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
    {
        memset(bytes, 0xA5, LONG_READ * BLOCK_SECTOR_SIZE + 1);
        long got = vfs_read(node, bytes, cases[i].count, cases[i].offset);
        if (got != (long)cases[i].count || memcmp(bytes, sectors + cases[i].offset, cases[i].count) != 0)
            panic("Block self-test: %zu bytes at %u read %ld bytes or different ones\n", cases[i].count,
                  (unsigned int)cases[i].offset, got);
        if (bytes[cases[i].count] != 0xA5)
            panic("Block self-test: %zu bytes at %u wrote past them\n", cases[i].count, (unsigned int)cases[i].offset);
    }
    // Two bytes inside a sector are the real thing, not just equal to the sector read
    if (vfs_read(node, bytes, 2, 1024 + SB_MAGIC) != 2 || le16(bytes) != EXT2_MAGIC)
        panic("Block self-test: no ext2 magic at byte %u of %s\n", 1024 + SB_MAGIC, path);

    // The end of the disk: a read across it is cut short, at or past it gives 0
    block_read_or_panic(dev, dev->sector_count - 1, 1, sectors);
    memset(bytes, 0xA5, 100);
    if (vfs_read(node, bytes, 100, size - 10) != 10 || memcmp(bytes, sectors + BLOCK_SECTOR_SIZE - 10, 10) != 0 ||
        bytes[10] != 0xA5)
        panic("Block self-test: a read across the end of %s was not cut to its last 10 bytes\n", path);
    if (vfs_read(node, bytes, 100, size) != 0 || vfs_read(node, bytes, 100, size + 1000) != 0 ||
        vfs_read(node, bytes, 100, UINT64_MAX) != 0 || vfs_read(node, bytes, 0, 0) != 0)
        panic("Block self-test: a read at or past the end of %s did not give 0\n", path);

    kfree(bytes);
    kfree(sectors);

    if (node->refs != refs)
        panic("Block self-test: %s's references leaked (%u → %u)\n", path, refs, node->refs);
    vfs_node_put(node);
}

void block_selftest(void)
{
    block_device_t *dev = block_test_disk();

    block_test_reads(dev);
    block_test_node(dev);

    // Registration: a name already there is refused, unknown names are not found
    if (block_register(dev) != -EEXIST)
        panic("Block self-test: registering %s twice did not return -EEXIST\n", dev->name);
    if (block_find("nope") != NULL)
        panic("Block self-test: block_find found a disk that does not exist\n");
}
