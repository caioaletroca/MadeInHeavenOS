#include <selftest/partition.h>
#include <driver/block.h>
#include <fs/vfs.h>
#include <mihos/errno.h>
#include <mihos/stat.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <kprintf.h>
#include <panic.h>

// part.img's layout (disk-tools/partition-disk.sh)
#define PART1_FIRST 2048
#define PART1_SECTORS 2048
#define PART4_FIRST 6144
#define PART4_SECTORS 2047

#define WRITE_SECTOR 100 // inside hdb1: disk sector PART1_FIRST + 100, no marker there

static uint8_t sector[BLOCK_SECTOR_SIZE];
static uint8_t other[BLOCK_SECTOR_SIZE];

/**
 * The disk holding partitions: the first of hda…hdd that has a "<name>1"
 * (hdb on QEMU: disk.img is hda, part.img hdb).
 */
static block_device_t *partition_test_disk(void)
{
    static const char *const names[] = {"hda", "hdb", "hdc", "hdd"};

    for (size_t i = 0; i < sizeof(names) / sizeof(names[0]); i++)
    {
        char first[BLOCK_NAME_MAX];
        snprintf(first, sizeof(first), "%s1", names[i]);

        block_device_t *disk = block_find(names[i]);
        if (disk != NULL && block_find(first) != NULL)
            return disk;
    }

    return NULL;
}

static block_device_t *partition_expect(block_device_t *disk, int slot, uint64_t sectors)
{
    char name[BLOCK_NAME_MAX];
    snprintf(name, sizeof(name), "%s%d", disk->name, slot);

    block_device_t *part = block_find(name);
    if (part == NULL)
        panic("Partition self-test: %s was not registered\n", name);
    if (part->sector_count != sectors)
        panic("Partition self-test: %s has %u sectors, not %u\n", name, (unsigned int)part->sector_count,
              (unsigned int)sectors);
    if (part->rdev != disk->rdev + (uint64_t)slot || MIHOS_MAJOR(part->rdev) != MIHOS_MAJOR(disk->rdev))
        panic("Partition self-test: %s is (%u, %u), not its disk's minor + %d\n", name, MIHOS_MAJOR(part->rdev),
              MIHOS_MINOR(part->rdev), slot);

    return part;
}

static void partition_expect_none(block_device_t *disk, int slot)
{
    char name[BLOCK_NAME_MAX];
    snprintf(name, sizeof(name), "%s%d", disk->name, slot);

    if (block_find(name) != NULL)
        panic("Partition self-test: %s exists, but its slot is %s\n", name, slot == 2 ? "empty" : "extended");
}

static void partition_read_or_panic(block_device_t *dev, uint64_t at, uint32_t count, void *buffer)
{
    int ret = block_read(dev, at, count, buffer);
    if (ret != 0)
        panic("Partition self-test: reading %u sectors at %u of %s failed: %d\n", (unsigned int)count,
              (unsigned int)at, dev->name, ret);
}

/**
 * Sector `at` of the partition starts with `text` (the script's markers)
 */
static void partition_expect_marker(block_device_t *part, uint64_t at, const char *text)
{
    partition_read_or_panic(part, at, 1, sector);
    if (memcmp(sector, text, strlen(text)) != 0)
        panic("Partition self-test: sector %u of %s does not start with '%s'\n", (unsigned int)at, part->name, text);
}

/**
 * The partition's edges: its first and last sectors are the markers, one
 * sector past the end (and a range running into it) is refused
 */
static void partition_test_range(block_device_t *part, const char *first, const char *last)
{
    partition_expect_marker(part, 0, first);
    partition_expect_marker(part, part->sector_count - 1, last);

    if (block_read(part, part->sector_count, 1, sector) != -EINVAL ||
        block_read(part, part->sector_count - 1, 2, sector) != -EINVAL ||
        block_write(part, part->sector_count, 1, sector) != -EINVAL)
        panic("Partition self-test: %s let a read or write past its end through\n", part->name);
}

/**
 * Reads and writes reach the disk at first_sector + sector: a multi-sector read
 * equals the disk's sectors there, and a write shows up on the disk
 */
static void partition_test_offset(block_device_t *disk, block_device_t *part)
{
    // The partition's last 2 sectors in one read equal the disk's same 2, one by one
    static uint8_t two[2 * BLOCK_SECTOR_SIZE];
    partition_read_or_panic(part, PART1_SECTORS - 2, 2, two);
    for (uint32_t i = 0; i < 2; i++)
    {
        uint64_t at = PART1_FIRST + PART1_SECTORS - 2 + i;
        partition_read_or_panic(disk, at, 1, other);
        if (memcmp(two + i * BLOCK_SECTOR_SIZE, other, BLOCK_SECTOR_SIZE) != 0)
            panic("Partition self-test: sector %u of a 2-sector read of %s is not %s's %u\n", (unsigned int)i,
                  part->name, disk->name, (unsigned int)at);
    }

    // A write through the partition lands at its first sector + WRITE_SECTOR on the disk, then is undone
    static uint8_t saved[BLOCK_SECTOR_SIZE];
    partition_read_or_panic(part, WRITE_SECTOR, 1, saved);

    for (size_t i = 0; i < BLOCK_SECTOR_SIZE; i++)
        sector[i] = (uint8_t)(i * 7 + 3);
    if (block_write(part, WRITE_SECTOR, 1, sector) != 0)
        panic("Partition self-test: writing sector %u of %s failed\n", WRITE_SECTOR, part->name);

    partition_read_or_panic(disk, PART1_FIRST + WRITE_SECTOR, 1, other);
    if (memcmp(sector, other, BLOCK_SECTOR_SIZE) != 0)
        panic("Partition self-test: a write to %s's sector %u is not at %s's %u\n", part->name, WRITE_SECTOR,
              disk->name, PART1_FIRST + WRITE_SECTOR);

    if (block_write(part, WRITE_SECTOR, 1, saved) != 0 || block_flush(part) != 0)
        panic("Partition self-test: putting back %s's sector %u failed\n", part->name, WRITE_SECTOR);
}

/**
 * /dev/<partition>: a block device node of the partition's size and number
 */
static void partition_test_node(block_device_t *part)
{
    char path[sizeof("/dev/") + BLOCK_NAME_MAX];
    snprintf(path, sizeof(path), "/dev/%s", part->name);

    vfs_node_t *node;
    int ret = vfs_lookup(path, &node);
    if (ret != 0)
        panic("Partition self-test: lookup of %s failed: %d\n", path, ret);

    if (node->type != VFS_BLOCKDEV || node->rdev != part->rdev ||
        node->size != part->sector_count * BLOCK_SECTOR_SIZE)
        panic("Partition self-test: %s is not a block device of %s's size and number\n", path, part->name);

    vfs_node_put(node);
}

void partition_selftest(void)
{
    block_device_t *disk = partition_test_disk();
    if (disk == NULL)
    {
        kprintf("partition self-test: no disk with partitions (part.img not attached), skipped\n");
        return;
    }

    block_device_t *part1 = partition_expect(disk, 1, PART1_SECTORS);
    block_device_t *part4 = partition_expect(disk, 4, PART4_SECTORS);
    partition_expect_none(disk, 2);
    partition_expect_none(disk, 3);

    partition_test_range(part1, "mihos-part1 first", "mihos-part1 last");
    partition_test_range(part4, "mihos-part4 first", "mihos-part4 last");

    partition_test_offset(disk, part1);

    partition_test_node(part1);
    partition_test_node(part4);
}
