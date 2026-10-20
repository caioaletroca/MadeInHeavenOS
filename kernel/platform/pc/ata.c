#include "ata.h"
#include <x86/io.h>
#include <driver/block.h>
#include <mihos/errno.h>
#include <stdint.h>
#include <string.h>
#include <kprintf.h>
#include <panic.h>

// ATA channel and control port definitions
#define ATA_PRIMARY_CHANNEL 0x1F0
#define ATA_PRIMARY_CONTROL 0x3F6
#define ATA_SECONDARY_CHANNEL 0x170
#define ATA_SECONDARY_CONTROL 0x376

// ATA register definitions
#define ATA_REG_DATA 0x00
#define ATA_REG_ERROR 0x01
#define ATA_REG_FEATURES 0x01
#define ATA_REG_SECTOR_COUNT 0x02
#define ATA_REG_LBA_LOW 0x03
#define ATA_REG_LBA_MID 0x04
#define ATA_REG_LBA_HIGH 0x05
#define ATA_REG_DRIVE 0x06
#define ATA_REG_COMMAND 0x07
#define ATA_REG_STATUS 0x07

// ATA command definitions
#define ATA_CMD_IDENTIFY 0xEC
#define ATA_CMD_READ_SECTORS 0x20

// ATA status register bits
#define ATA_SR_BSY 0x80
#define ATA_SR_DRDY 0x40
#define ATA_SR_DF 0x20
#define ATA_SR_DSC 0x10
#define ATA_SR_DRQ 0x08
#define ATA_SR_CORR 0x04
#define ATA_SR_IDX 0x02
#define ATA_SR_ERR 0x01

/**
 * ATA drive structure
 */
typedef struct ata_drive
{
    uint16_t io;
    uint16_t control;
    uint8_t slave;
    block_device_t block;
} ata_drive_t;

static ata_drive_t ata_drives[4];

/**
 * @brief Introduce a small delay for ATA operations by reading the alternate status register multiple times.
 *
 * @param drive Pointer to the ATA drive structure.
 */
static void ata_delay(ata_drive_t *drive)
{
    inb(drive->control);
    inb(drive->control);
    inb(drive->control);
    inb(drive->control);
}

/**
 * @brief Wait for the ATA drive to be ready by polling the status register.
 *
 * @param drive Pointer to the ATA drive structure.
 * @return ATA status on success, -EIO on error.
 */
static int ata_wait_ready(ata_drive_t *drive)
{
    uint8_t status;
    int i = 0;
    do
    {
        status = inb(drive->io + ATA_REG_STATUS);
        i++;

        // Timeout to prevent infinite loop
        if (i > 1000000)
            return -EIO;
    } while (status & ATA_SR_BSY);

    return (int)status;
}

/**
 * @brief Wait for the ATA drive to have data ready by polling the status register.
 *
 * @param drive Pointer to the ATA drive structure.
 * @return ATA status on success, -EIO on error.
 */
static int ata_wait_data(ata_drive_t *drive)
{
    int status = ata_wait_ready(drive);
    if (status < 0)
        return status;

    if (status & ATA_SR_ERR || status & ATA_SR_DF || !(status & ATA_SR_DRQ))
        return -EIO;

    return status;
}

/**
 * @brief Read data from the ATA drive into the provided buffer.
 *
 * @param drive Pointer to the ATA drive structure.
 * @param buffer Pointer to the buffer to store the read data.
 * @param size Number of bytes to read.
 * @return 0 on success, -EIO on error.
 */
static int ata_read_data(ata_drive_t *drive, uint16_t *buffer, size_t size)
{
    // Wait for the drive to be ready before reading data
    if (ata_wait_data(drive) < 0)
        return -EIO;

    uint16_t *buf = buffer;
    for (size_t i = 0; i < size / 2; i++)
        buf[i] = inw(drive->io + ATA_REG_DATA);

    return 0;
}

/**
 * @brief Identify the ATA drive and retrieve its information.
 *
 * @param drive Pointer to the ATA drive structure.
 * @return 0 on success, -1 on failure.
 */
static int ata_identify(ata_drive_t *drive)
{
    outb(drive->io + ATA_REG_DRIVE, 0xA0 | (drive->slave << 4));
    ata_delay(drive);

    // Zero out the sector count and LBA registers before sending the IDENTIFY command
    outb(drive->io + ATA_REG_SECTOR_COUNT, 0);
    outb(drive->io + ATA_REG_LBA_LOW, 0);
    outb(drive->io + ATA_REG_LBA_MID, 0);
    outb(drive->io + ATA_REG_LBA_HIGH, 0);

    outb(drive->io + ATA_REG_COMMAND, ATA_CMD_IDENTIFY);

    uint8_t status = inb(drive->io + ATA_REG_STATUS);
    // Floating bus detection: if the status register reads 0 or 0xFF, the drive is not present
    if (status == 0 || status == 0xFF)
        return -1;

    if (ata_wait_ready(drive) < 0)
        return -1;

    // Check if the drive is ATA or ATAPI by reading the LBA mid and high registers
    uint16_t lba_mid = inb(drive->io + ATA_REG_LBA_MID);
    uint16_t lba_high = inb(drive->io + ATA_REG_LBA_HIGH);
    if (lba_mid != 0 || lba_high != 0)
        return -1;

    uint16_t word[256];
    int ret = ata_read_data(drive, word, sizeof(word));
    if (ret < 0)
        return -1;

    uint64_t count = word[60] | ((uint32_t)word[61] << 16);
    // A count of 0 means no LBA support, so the drive is not usable
    if (count == 0)
        return -1;

    drive->block.sector_count = count;

    return 0;
}

/**
 * @brief Read data from the ATA drive.
 *
 * @param dev Pointer to the block device structure.
 * @param sector Starting sector to read from.
 * @param count Number of sectors to read.
 * @param buffer Pointer to the buffer to store the read data.
 * @return 0 on success, -EINVAL on invalid parameters.
 */
static int ata_read(block_device_t *dev, uint64_t sector, uint32_t count, void *buffer)
{
    ata_drive_t *drive = dev->private;

    // 28-bit LBA reaches sectors 0 .. 2^28 - 1 (128 GiB); bigger disks need LBA48
    if (sector + count > (1u << 28))
        return -EINVAL;

    uint16_t *words = buffer;
    uint64_t lba = sector;
    uint32_t remaining = count;
    while (remaining > 0)
    {
        // The count register is 8 bits: one command reads at most 256 sectors (256 is written as 0)
        uint32_t n = remaining < 256 ? remaining : 256;

        if (ata_wait_ready(drive) < 0)
            return -EIO;

        // Select the drive in LBA mode (0xE0) with LBA bits 24-27, then give it 400 ns to switch
        outb(drive->io + ATA_REG_DRIVE, (uint8_t)(0xE0 | (drive->slave << 4) | ((lba >> 24) & 0x0F)));
        ata_delay(drive);

        outb(drive->io + ATA_REG_SECTOR_COUNT, (uint8_t)(n & 0xFF));
        outb(drive->io + ATA_REG_LBA_LOW, (uint8_t)(lba & 0xFF));
        outb(drive->io + ATA_REG_LBA_MID, (uint8_t)((lba >> 8) & 0xFF));
        outb(drive->io + ATA_REG_LBA_HIGH, (uint8_t)((lba >> 16) & 0xFF));
        outb(drive->io + ATA_REG_COMMAND, ATA_CMD_READ_SECTORS);
        ata_delay(drive);

        // The drive hands over one sector at a time: wait for DRQ before each one
        for (uint32_t i = 0; i < n; i++)
        {
            if (ata_read_data(drive, words, BLOCK_SECTOR_SIZE) < 0)
                return -EIO;

            words += BLOCK_SECTOR_SIZE / 2;
        }

        lba += n;
        remaining -= n;
    }

    return 0;
}

static const block_ops_t ata_block_ops = {
    .read = ata_read,
};

static const char *const names[4] = {"hda", "hdb", "hdc", "hdd"};
void ata_init(void)
{
    outb(ATA_PRIMARY_CONTROL, 0x02);
    outb(ATA_SECONDARY_CONTROL, 0x02);

    // Initialize and identify all ATA drives
    for (int i = 0; i < 4; i++)
    {
        ata_drive_t *drive = &ata_drives[i];
        drive->io = i < 2 ? ATA_PRIMARY_CHANNEL : ATA_SECONDARY_CHANNEL;
        drive->control = i < 2 ? ATA_PRIMARY_CONTROL : ATA_SECONDARY_CONTROL;
        drive->slave = i % 2;

        if (ata_identify(drive) < 0)
            continue;

        strlcpy(drive->block.name, names[i], sizeof(drive->block.name));
        drive->block.ops = &ata_block_ops;
        drive->block.private = drive;

        if (block_register(&drive->block) < 0)
            panic("ata: cannot register %s\n", names[i]);

        kprintf("ata: %s, %u MiB\n", names[i], (unsigned int)(drive->block.sector_count / 2048));
    }
}