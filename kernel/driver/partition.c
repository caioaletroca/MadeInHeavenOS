#include <driver/partition.h>
#include <driver/block.h>
#include <mm/kmalloc.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <kprintf.h>

#define MBR_ENTRIES_OFFSET 446
#define MBR_ENTRIES 4
#define MBR_SIGNATURE_OFFSET 510

#define MBR_TYPE_EMPTY 0x00
#define MBR_TYPE_GPT 0xEE // a GPT disk's "protective" entry: the real table is elsewhere

/**
 * One MBR entry, as on the disk (16 bytes, little-endian; the fields fall on
 * their natural alignment, so no packing is needed)
 */
typedef struct mbr_entry
{
    uint8_t status;       // 0x80 bootable, 0x00 not; anything else: not an MBR
    uint8_t chs_first[3]; // cylinder/head/sector addressing, from before LBA: unused
    uint8_t type;
    uint8_t chs_last[3];
    uint32_t first_sector;
    uint32_t sector_count;
} mbr_entry_t;

_Static_assert(sizeof(mbr_entry_t) == 16, "an MBR entry is 16 bytes");

/**
 * A partition: a block device that is a range of a disk's sectors
 */
typedef struct partition
{
    block_device_t block;  // what the rest of the kernel sees (hdb1)
    block_device_t *disk;  // the disk it is on (hdb)
    uint64_t first_sector; // where it starts on that disk
} partition_t;

/**
 * @brief Read from a partition
 *
 * @param dev The partition block device
 * @param sector The starting sector to read from
 * @param count The number of sectors to read
 * @param buffer The buffer to store the read data
 * @return 0 on success, negative on failure
 */
static int partition_read(block_device_t *dev, uint64_t sector, uint32_t count, void *buffer)
{
    partition_t *partition = dev->private;
    return block_read(partition->disk, partition->first_sector + sector, count, buffer);
}

/**
 * @brief Write to a partition
 *
 * @param dev The partition block device
 * @param sector The starting sector to write to
 * @param count The number of sectors to write
 * @param buffer The buffer containing the data to write
 * @return 0 on success, negative on failure
 */
static int partition_write(block_device_t *dev, uint64_t sector, uint32_t count, const void *buffer)
{
    partition_t *partition = dev->private;
    return block_write(partition->disk, partition->first_sector + sector, count, buffer);
}

/**
 * @brief Flush a partition's data to the disk
 *
 * @param dev The partition block device
 * @return 0 on success, negative on failure
 */
static int partition_flush(block_device_t *dev)
{
    partition_t *partition = dev->private;
    return block_flush(partition->disk);
}

static const block_ops_t partition_ops = {
    .read = partition_read,
    .write = partition_write,
    .flush = partition_flush,
};

/**
 * @brief Check if an MBR type is an extended partition
 *
 * @param type The MBR partition type
 * @return true if it is an extended partition, false otherwise
 */
static bool mbr_type_extended(uint8_t type)
{
    return type == 0x05 || type == 0x0F || type == 0x85; // CHS, LBA and Linux's extended types
}

/**
 * @brief Check the MBR magic signature
 *
 * @param table The MBR table
 * @return true if the magic is valid, false otherwise
 */
static bool mbr_check_magic(const uint8_t *table)
{
    return table[MBR_SIGNATURE_OFFSET] == 0x55 && table[MBR_SIGNATURE_OFFSET + 1] == 0xAA;
}

/**
 * @brief Add a partition to a disk
 *
 * @param disk The disk block device
 * @param slot The partition slot number
 * @param entry The MBR partition entry
 */
static void partition_add(block_device_t *disk, int slot, const mbr_entry_t *entry)
{
    if (entry->type == MBR_TYPE_EMPTY)
        return;

    if (mbr_type_extended(entry->type))
    {
        kprintf("partition: %s%d is extended: logical partitions are not read\n", disk->name, slot);
        return;
    }

    if (entry->first_sector == 0 ||
        entry->sector_count == 0 ||
        entry->first_sector >= disk->sector_count ||
        entry->sector_count > disk->sector_count - entry->first_sector)
    {
        kprintf("partition: %s%d is outside the disk, skipped\n", disk->name, slot);
        return;
    }

    partition_t *partition = kzalloc(sizeof(partition_t));
    if (partition == NULL)
    {
        kprintf("partition: %s%d failed to allocate memory\n", disk->name, slot);
        return;
    }

    snprintf(partition->block.name, sizeof(partition->block.name), "%s%d", disk->name, slot);
    partition->block.private = partition;
    partition->block.rdev = disk->rdev + slot;
    partition->block.sector_count = entry->sector_count;
    partition->block.ops = &partition_ops;

    partition->disk = disk;
    partition->first_sector = entry->first_sector;

    int ret = block_register(&partition->block);
    if (ret < 0)
    {
        kprintf("partition: %s%d failed to register block device\n", disk->name, slot);
        kfree(partition);
        return;
    }

    kprintf("partition: %s, %u MiB at sector %u\n", partition->block.name, (entry->sector_count / 2048), entry->first_sector);
}

static void partition_scan(block_device_t *disk)
{
    static uint8_t table[BLOCK_SECTOR_SIZE];

    long bytes = block_read_bytes(disk, table, sizeof(table), 0);
    if (bytes != 512)
    {
        kprintf("partition: %s failed to read MBR\n", disk->name);
        return;
    }

    // No signature: no partition table (disk.img: an ext2 over the whole disk starts with zeros)
    if (!mbr_check_magic(table))
        return;

    // The four entries are contiguous on the disk: one copy (memcpy: no unaligned casts)
    mbr_entry_t entries[MBR_ENTRIES];
    memcpy(entries, table + MBR_ENTRIES_OFFSET, sizeof(entries));

    // Check all four before adding any: a FAT boot sector also ends in 55 AA, and a sector
    // that is not a table must add nothing (not slot 1, then give up at slot 2)
    for (int i = 0; i < MBR_ENTRIES; i++)
    {
        mbr_entry_t *entry = &entries[i];

        if (entry->status != 0x00 && entry->status != 0x80)
            return;
        if (entry->type == MBR_TYPE_GPT)
        {
            kprintf("partition: %s has a GPT: not read yet\n", disk->name);
            return;
        }
    }

    for (int i = 0; i < MBR_ENTRIES; i++)
        partition_add(disk, i + 1, &entries[i]);
}

void partition_init(void)
{
    // Partitions register as block devices too: count the disks first, so the scan never
    // reaches a partition it just added (no tables inside partitions)
    size_t disks = 0;
    while (block_at(disks) != NULL)
        disks++;

    for (size_t i = 0; i < disks; i++)
        partition_scan(block_at(i));
}