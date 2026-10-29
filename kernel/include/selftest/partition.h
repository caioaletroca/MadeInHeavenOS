#ifndef _SELFTEST_PARTITION_H_
#define _SELFTEST_PARTITION_H_

/**
 * Self-test for MBR partitions, against the partition test disk (part.img,
 * see disk-tools/partition-disk.sh) attached next to disk.img: which slots
 * became devices, their sizes and numbers, the ext2 on partition 1, the
 * markers at partition 4's first and last sectors, the range checks, a write
 * landing at the right disk sector, and /dev/<partition>. Skipped, with a
 * message, when no disk has partitions
 * (a Bochs setup without part.img).
 */
void partition_selftest(void);

#endif // _SELFTEST_PARTITION_H_
