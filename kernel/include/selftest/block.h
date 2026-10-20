#ifndef _SELFTEST_BLOCK_H_
#define _SELFTEST_BLOCK_H_

/**
 * Self-test for the block layer and the disk driver, against the ext2 test
 * disk (disk.img) the emulators attach: the ext2 superblock read back from
 * sector 2, multi-sector reads (also across a 256-sector command) equal to
 * single ones, the range checks, and registration refusing a duplicate; the
 * block cache (leases, hits and misses, LRU eviction, -ENOBUFS); /dev/<disk>
 * read at byte offsets.
 * Panics if no disk was found.
 */
void block_selftest(void);

#endif // _SELFTEST_BLOCK_H_
