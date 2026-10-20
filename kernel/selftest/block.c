#include <selftest/block.h>
#include <driver/block.h>
#include <mm/kmalloc.h>
#include <mihos/errno.h>
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

void block_selftest(void)
{
    block_device_t *dev = block_test_disk();

    block_test_reads(dev);

    // Registration: a name already there is refused, unknown names are not found
    if (block_register(dev) != -EEXIST)
        panic("Block self-test: registering %s twice did not return -EEXIST\n", dev->name);
    if (block_find("nope") != NULL)
        panic("Block self-test: block_find found a disk that does not exist\n");
}
