#ifndef _SELFTEST_EXT2_H_
#define _SELFTEST_EXT2_H_

/**
 * Self-test for ext2 on the test disk mounted at /mnt: the root and its
 * listing (the symlink skipped), lookups, the node cache (one node per inode,
 * directories held by their children and let go again), reads through direct,
 * single and double indirect blocks and a hole, read-only opens; and broken
 * copies of the disk (bytes replaced by an overlay device) refused at mount or
 * giving -EIO in directories. Panics on failure.
 */
void ext2_selftest(void);

#endif // _SELFTEST_EXT2_H_
